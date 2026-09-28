static const float kWaterF0 = 0.02;
static const float kInverseFourPi = 0.0795775;
static const float kFoamRoughness = 0.8;
static const float kRoughnessExponent = 0.25;
static const float kMinRoughnessMoment = 1e-8;
static const float kRefractionUvPerSlopeYard = 0.005;
static const float kMaxRefractionDepth = 50;
static const float kShoreFadePerYard = 2;
static const float kFoamStateGain = 10;
static const float kFoamWrap = 0.25;
static const float kFoamCrestSharpness = 8;
static const float kCrestColourBlend = 0.5;
static const float kMinCosine = 1e-5;
static const float kMinPhaseDenominator = 1e-6;
static const float kMinSunAlpha = 1e-3;
static const float kSunDiscAlpha = 0.02;
static const float kMinVisibilityDenominator = 1e-5;

struct Refraction
{
    float3 scene;
    float pathDepth;
};

float FresnelSchlick(float cosine)
{
    return kWaterF0 + (1 - kWaterF0) * pow(1 - saturate(cosine), 5);
}

float PerceptualRoughness(float parameter, float variance)
{
    return saturate(pow(max(parameter * abs(parameter) + variance, kMinRoughnessMoment), kRoughnessExponent));
}

float EnvironmentBrdf(float roughness, float NoV)
{
    float4 r = roughness * float4(-1, -0.0275, -0.572, 0.022) + float4(1, 0.0425, 1.04, -0.04);
    float a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;
    float2 ab = float2(-1.04, 1.04) * a004 + r.zw;
    return kWaterF0 * ab.x + ab.y;
}

Refraction RefractScene(WaterPixel w, float2 slope)
{
    float2 refractedUv = w.uv + slope * kRefractionUvPerSlopeYard * min(kMaxRefractionDepth, w.sceneZ - w.waterZ);
    float refractedZ = CopiedViewDepth(sSceneDepth, refractedUv);
    float2 colourUv = refractedZ < w.waterZ ? w.uv : refractedUv;
    Refraction r;
    r.scene = GammaToLinear(tex2Dlod(sSceneColour, float4(colourUv, 0, 0)).rgb);
    r.pathDepth = max(0, max(refractedZ, w.sceneZ) - w.waterZ);
    return r;
}

float3 Transmittance(float pathDepth)
{
    return exp(-AbsorptionPerYard() * pathDepth);
}

float3 InScattering(float3 V, float NoV, float crestTilt, float oxygen, float3 transmittance)
{
    float g = ScatteringAnisotropy();
    float cosTheta = -dot(V, ToLight());
    float phase = (1 - g * g) / pow(max(1 + g * g - 2 * g * cosTheta, kMinPhaseDenominator), 1.5);
    float4 intensity = cScatteringIntensities;
    float viewPart = NoV * NoV * intensity.x + intensity.y * crestTilt;
    float sunPart = SunTransmission() * intensity.z;
    float oxygenPart = saturate(ToLight().z) * oxygen * intensity.w;
    float normaliser = 1 / max(1, sunPart + viewPart + oxygenPart);
    float3 colour = lerp(TroughScatteringColour(), CrestScatteringColour(), kCrestColourBlend);
    float3 anisotropic = phase * SunVisibility() * SunColour() * viewPart * normaliser;
    float3 isotropic = (oxygenPart * normaliser + sunPart) * IsotropicLight();
    return (1 - transmittance) * kInverseFourPi * (isotropic + anisotropic) * colour;
}

float3 FoamLight(float3 N, float foamState)
{
    float3 L = ToLight();
    float NoL = clamp(dot(N, L), kMinCosine, 1);
    float state = saturate(foamState * kFoamStateGain);
    float crest = saturate(state * state * (3 - 2 * state) * L.z);
    float diffuse = lerp(saturate(NoL + kFoamWrap), pow(NoL, kFoamCrestSharpness), crest) * SunVisibility();
    return lerp(Ambient(), SunColour(), diffuse);
}

float SunGlint(float roughness, float3 N, float3 V, float3 L)
{
    float3 H = normalize(V + L);
    float NoH = saturate(dot(N, H));
    float NoL = saturate(dot(N, L));
    float NoV = max(dot(N, V), kMinCosine);
    float alpha = max(roughness * roughness, kMinSunAlpha);
    float widened = saturate(alpha + kSunDiscAlpha);
    float widened2 = widened * widened;
    float lobe = NoH * NoH * (widened2 - 1) + 1;
    float energy = alpha / widened;
    float distribution = widened2 / (kPi * lobe * lobe) * energy * energy;
    float visibility = 0.5 / max(NoL * (NoV * (1 - alpha) + alpha) + NoV * (NoL * (1 - alpha) + alpha),
                                 kMinVisibilityDenominator);
    return distribution * visibility * FresnelSchlick(dot(V, H)) * NoL;
}
