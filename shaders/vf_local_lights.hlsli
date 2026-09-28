float4 cLocalLightControl : register(c53);
sampler2D sLocalLights : register(s8);

static const float kFirstQuadratureFraction = 0.2113248654;
static const float kQuadratureFractionSpacing = 0.5773502692;
static const float kBeyondEveryLight = 1e30;

struct ChordCoverage
{
    float nextEndpoint;
    bool covered;
    float firstLight;
    float lightEnd;
};

struct LocalLight
{
    float3 position;
    float radius;
    float projected;
    float2 chord;
};

struct LayerLightWeights
{
    float4 anisotropic;
    float4 isotropic;
};

struct LightSampleDensities
{
    float4 anisotropic;
    float isotropic;
};

float LocalLightCount()
{
    return cLocalLightControl.x;
}

float LocalLightReachLimit()
{
    return cLocalLightControl.y;
}

float4 LocalLightParameter(int index)
{
    float texel = (index + 0.5) / 32.0;
    return tex2Dlod(sLocalLights, float4(texel, texel, texel, -texel));
}

float3 LocalLightColour(int light)
{
    return LocalLightParameter(light * 3 + 1).rgb;
}

float3 LocalLightAttenuation(int light)
{
    return LocalLightParameter(light * 3 + 2).xyz;
}

LocalLight LoadLocalLight(int index, float3 viewDirection)
{
    float4 positionRadius = LocalLightParameter(index * 3);
    LocalLight light;
    light.position = positionRadius.xyz;
    light.radius = positionRadius.w;
    light.projected = dot(light.position, viewDirection);
    float perpendicularSquared = max(dot(light.position, light.position) - light.projected * light.projected, 0);
    float discriminant = light.radius * light.radius - perpendicularSquared;
    float halfChord = sqrt(max(discriminant, 0));
    light.chord = discriminant > 0 ? float2(light.projected - halfChord, light.projected + halfChord)
                                   : float2(kBeyondEveryLight, -kBeyondEveryLight);
    return light;
}

float QuadratureFraction(int sample)
{
    return kFirstQuadratureFraction + sample * kQuadratureFractionSpacing;
}

ChordCoverage ChordCoverageFrom(ChordCoverage previous, float3 viewDirection, float distanceAlongRay,
                                float marchLength)
{
    ChordCoverage coverage;
    coverage.nextEndpoint = kBeyondEveryLight;
    coverage.covered = false;
    coverage.firstLight = previous.lightEnd;
    coverage.lightEnd = previous.firstLight;
    [loop] for (int index = previous.firstLight; index < previous.lightEnd; ++index)
    {
        float2 chord = LoadLocalLight(index, viewDirection).chord;
        float endpoint = chord.x > distanceAlongRay ? chord.x : chord.y;
        coverage.nextEndpoint = endpoint > distanceAlongRay ? min(coverage.nextEndpoint, endpoint)
                                                            : coverage.nextEndpoint;
        coverage.covered = coverage.covered || (chord.x <= distanceAlongRay && chord.y > distanceAlongRay);
        [flatten] if (chord.y > distanceAlongRay && chord.x < marchLength)
        {
            coverage.firstLight = min(coverage.firstLight, index);
            coverage.lightEnd = index + 1;
        }
    }
    return coverage;
}

bool StepMeetsLocalLights(ChordCoverage coverage, float stepEnd)
{
    return coverage.covered || coverage.nextEndpoint < stepEnd;
}

float LayerLightDensityScale(FogLayer layer, float skyMask, float upward)
{
    return layer.density * SkyDensityScale(layer, skyMask, upward) * ShadowDensityScale(layer);
}

float AnisotropicPhaseWeight(FogLayer layer)
{
    float forward = 1 - layer.g;
    return (1 - layer.isotropic) * forward * forward * forward;
}

float4 LayerLightDensityScales(float skyMask, float upward)
{
    return float4(LayerLightDensityScale(LoadConstantFogLayer(0), skyMask, upward),
                  LayerLightDensityScale(LoadConstantFogLayer(1), skyMask, upward),
                  LayerLightDensityScale(LoadConstantFogLayer(2), skyMask, upward),
                  LayerLightDensityScale(LoadConstantFogLayer(3), skyMask, upward));
}

LayerLightWeights LayerLightWeightsAlongRay(float skyMask, float upward)
{
    float4 densityScales = LayerLightDensityScales(skyMask, upward);
    LayerLightWeights weights;
    weights.anisotropic = densityScales *
        float4(AnisotropicPhaseWeight(LoadConstantFogLayer(0)), AnisotropicPhaseWeight(LoadConstantFogLayer(1)),
               AnisotropicPhaseWeight(LoadConstantFogLayer(2)), AnisotropicPhaseWeight(LoadConstantFogLayer(3)));
    weights.isotropic = densityScales *
        float4(LoadConstantFogLayer(0).isotropic, LoadConstantFogLayer(1).isotropic,
               LoadConstantFogLayer(2).isotropic, LoadConstantFogLayer(3).isotropic);
    return weights;
}

bool LayerLitInStep(FogLayer layer, float stepStart, float stepEnd)
{
    float layerStart = max(stepStart, layer.start);
    return min(stepEnd, layer.limit) - layerStart > 0 && layer.density > 0 &&
           (LocalLightReachLimit() <= 0 || layerStart < LocalLightReachLimit());
}

bool4 LayersLitInStep(float stepStart, float stepEnd)
{
    return bool4(LayerLitInStep(LoadConstantFogLayer(0), stepStart, stepEnd),
                 LayerLitInStep(LoadConstantFogLayer(1), stepStart, stepEnd),
                 LayerLitInStep(LoadConstantFogLayer(2), stepStart, stepEnd),
                 LayerLitInStep(LoadConstantFogLayer(3), stepStart, stepEnd));
}

bool LayerCoversInterval(FogLayer layer, float2 interval)
{
    return layer.start <= interval.x && layer.limit >= interval.y;
}

bool4 LayersCoverInterval(float2 interval)
{
    return bool4(LayerCoversInterval(LoadConstantFogLayer(0), interval),
                 LayerCoversInterval(LoadConstantFogLayer(1), interval),
                 LayerCoversInterval(LoadConstantFogLayer(2), interval),
                 LayerCoversInterval(LoadConstantFogLayer(3), interval));
}

float LayerStart(int layer)
{
    return layer == 0 ? LoadConstantFogLayer(0).start
         : layer == 1 ? LoadConstantFogLayer(1).start
         : layer == 2 ? LoadConstantFogLayer(2).start : LoadConstantFogLayer(3).start;
}

float LayerLimit(int layer)
{
    return layer == 0 ? LoadConstantFogLayer(0).limit
         : layer == 1 ? LoadConstantFogLayer(1).limit
         : layer == 2 ? LoadConstantFogLayer(2).limit : LoadConstantFogLayer(3).limit;
}

float4 LayerSelector(int layer)
{
    return layer == int4(0, 1, 2, 3) ? 1 : 0;
}

float2 LayerIntervalWithin(float2 interval, int layer)
{
    return float2(max(interval.x, LayerStart(layer)), min(interval.y, LayerLimit(layer)));
}

float PhaseFalloff(FogLayer layer, float cosAngle)
{
    float inverseDistance = rsqrt(max(1 + layer.g * (layer.g - 2 * cosAngle), 1e-6));
    return inverseDistance * inverseDistance * inverseDistance;
}

float4 LayerPhaseFalloffs(float cosAngle)
{
    return float4(PhaseFalloff(LoadConstantFogLayer(0), cosAngle), PhaseFalloff(LoadConstantFogLayer(1), cosAngle),
                  PhaseFalloff(LoadConstantFogLayer(2), cosAngle), PhaseFalloff(LoadConstantFogLayer(3), cosAngle));
}

float4 DensityProfiles(MarchRay ray, float distanceAlongRay)
{
    float variation = DensityVariationAlongRay(ray, distanceAlongRay);
    return float4(DensityProfile(LoadConstantFogLayer(0), ray, distanceAlongRay, variation),
                  DensityProfile(LoadConstantFogLayer(1), ray, distanceAlongRay, variation),
                  DensityProfile(LoadConstantFogLayer(2), ray, distanceAlongRay, variation),
                  DensityProfile(LoadConstantFogLayer(3), ray, distanceAlongRay, variation));
}

LightSampleDensities SampleDensities(MarchRay ray, float distanceAlongRay, float4 layerMask,
                                     LayerLightWeights weights)
{
    float4 densities = DensityProfiles(ray, distanceAlongRay) * layerMask;
    LightSampleDensities sample;
    sample.anisotropic = densities * weights.anisotropic;
    sample.isotropic = dot(densities, weights.isotropic);
    return sample;
}

float LightSampleScattering(LocalLight light, float3 attenuation, float3 viewDirection, float distanceAlongRay,
                            LightSampleDensities densities)
{
    float3 toLight = light.position - viewDirection * distanceAlongRay;
    float distanceSquared = dot(toLight, toLight);
    float inverseDistance = rsqrt(max(distanceSquared, 1e-6));
    float lightDistance = max(distanceSquared, 1e-6) * inverseDistance;
    float falloff = rcp(max(attenuation.x + attenuation.y * lightDistance + attenuation.z * distanceSquared, 1));
    float boundaryFade = saturate((light.radius - lightDistance) * 4 / max(light.radius, 1e-3));
    float cosAngle = (light.projected - distanceAlongRay) * inverseDistance;
    return falloff * boundaryFade *
           (densities.isotropic + dot(LayerPhaseFalloffs(cosAngle), densities.anisotropic));
}

float IntervalScattering(LocalLight light, float3 attenuation, float2 interval, float4 layerMask,
                         LayerLightWeights weights, MarchRay ray)
{
    float quadrature = 0;
    [loop] for (int sample = 0; sample < 2; ++sample)
    {
        float distanceAlongRay = lerp(interval.x, interval.y, QuadratureFraction(sample));
        quadrature += LightSampleScattering(light, attenuation, ray.viewDirection, distanceAlongRay,
                                            SampleDensities(ray, distanceAlongRay, layerMask, weights));
    }
    return 0.5 * quadrature * (interval.y - interval.x);
}

float2 PieceInterval(int piece, float2 interval)
{
    return piece == 0 ? interval : LayerIntervalWithin(interval, piece - 1);
}

float4 PieceLayers(int piece, float2 interval, float2 pieceInterval, float4 litLayers)
{
    return piece == 0 ? (LayersCoverInterval(interval) ? litLayers : 0)
                      : (any(pieceInterval != interval) ? litLayers * LayerSelector(piece - 1) : 0);
}

float LightOwnIntervalsScattering(LocalLight light, float3 attenuation, float2 interval, bool wholeStep,
                                  bool layerBoundaryInStep, float4 litLayers, LayerLightWeights weights, MarchRay ray)
{
    float scattering = 0;
    int lastPiece = layerBoundaryInStep ? kFogLayers : 0;
    [loop] for (int piece = wholeStep ? 1 : 0; piece <= lastPiece; ++piece)
    {
        float2 pieceInterval = PieceInterval(piece, interval);
        float4 pieceLayers = PieceLayers(piece, interval, pieceInterval, litLayers);
        [branch] if (pieceInterval.y > pieceInterval.x && any(pieceLayers != 0))
            scattering += IntervalScattering(light, attenuation, pieceInterval, pieceLayers, weights, ray);
    }
    return scattering;
}

bool ChordCoversStep(float2 chord, float2 step)
{
    return chord.x <= step.x && chord.y >= step.y;
}

float3 WholeStepLightScattering(ChordCoverage coverage, float4 stepLayers, LayerLightWeights weights, MarchRay ray,
                                float2 step)
{
    float first = lerp(step.x, step.y, QuadratureFraction(0));
    float second = lerp(step.x, step.y, QuadratureFraction(1));
    LightSampleDensities atFirst;
    LightSampleDensities atSecond;
    [loop] for (int sample = 0; sample < 2; ++sample)
    {
        LightSampleDensities densities = SampleDensities(ray, sample == 0 ? first : second, stepLayers, weights);
        [flatten] if (sample == 0)
            atFirst = densities;
        else
            atSecond = densities;
    }
    float3 scattering = 0;
    [loop] for (int index = coverage.firstLight; index < coverage.lightEnd; ++index)
    {
        LocalLight light = LoadLocalLight(index, ray.viewDirection);
        [branch] if (ChordCoversStep(light.chord, step))
        {
            float3 attenuation = LocalLightAttenuation(index);
            scattering += LocalLightColour(index) *
                          (LightSampleScattering(light, attenuation, ray.viewDirection, first, atFirst) +
                           LightSampleScattering(light, attenuation, ray.viewDirection, second, atSecond));
        }
    }
    return 0.5 * (step.y - step.x) * scattering;
}

float3 LocalLightScattering(ChordCoverage coverage, LayerLightWeights weights, MarchRay ray, float stepStart,
                            float stepEnd)
{
    float2 step = float2(stepStart, stepEnd);
    float4 litLayers = LayersLitInStep(stepStart, stepEnd) ? 1 : 0;
    bool4 layersCoverStep = LayersCoverInterval(step);
    bool layerBoundaryInStep = any(layersCoverStep ? 0 : litLayers);
    bool lightCoversStep = coverage.covered;
    float3 scattering = 0;
    [branch] if (coverage.nextEndpoint < stepEnd || layerBoundaryInStep)
    {
        lightCoversStep = false;
        [loop] for (int index = coverage.firstLight; index < coverage.lightEnd; ++index)
        {
            LocalLight light = LoadLocalLight(index, ray.viewDirection);
            float2 interval = float2(max(stepStart, light.chord.x), min(stepEnd, light.chord.y));
            bool wholeStep = ChordCoversStep(light.chord, step);
            lightCoversStep = lightCoversStep || wholeStep;
            float amount = 0;
            [branch] if (interval.y > interval.x && (!wholeStep || layerBoundaryInStep))
                amount = LightOwnIntervalsScattering(light, LocalLightAttenuation(index), interval, wholeStep,
                                                     layerBoundaryInStep, litLayers, weights, ray);
            [branch] if (amount != 0)
                scattering += LocalLightColour(index) * amount;
        }
    }
    float4 stepLayers = layersCoverStep ? litLayers : 0;
    [branch] if (lightCoversStep && any(stepLayers != 0))
        scattering += WholeStepLightScattering(coverage, stepLayers, weights, ray, step);
    return scattering;
}
