#include "vf_common.hlsli"
#include "vw_constants.hlsli"
#include "vw_surface.hlsli"
#include "vw_optics.hlsli"
#include "vw_reflection.hlsli"

static const float kDebugNormal = 1;
static const float kDebugFoamCoverage = 2;
static const float kDebugTransmittance = 3;
static const float kDebugReflection = 4;

float3 ClassColour(float waterClass)
{
    float3 primaries = saturate(1 - abs(waterClass - float3(1, 2, 3)));
    return primaries + saturate(1 - abs(waterClass - 4)) * float3(1, 1, 0);
}

float3 DebugColour(float3 N, float4 foamAlbedo, float3 transmittance, float3 reflected)
{
    float view = WaterDebugView();
    if (view < kDebugNormal + 0.5)
        return N * 0.5 + 0.5;
    if (view < kDebugFoamCoverage + 0.5)
        return foamAlbedo.aaa;
    if (view < kDebugTransmittance + 0.5)
        return transmittance;
    if (view < kDebugReflection + 0.5)
        return LinearToGamma(reflected);
    return ClassColour(WaterClassIndex());
}

float4 main(float2 pixelIndex : VPOS) : COLOR0
{
    float2 pixel = pixelIndex + 0.5;
    WaterPixel w = ReconstructWaterPixel(pixel);
    WaveState waves = SampleWaves(w);
    float4 foamAlbedo = FoamAlbedo(w, waves);

    float3 N = normalize(float3(-waves.moments.xy, 1));
    float3 V = w.toCamera;
    float3 L = ToLight();
    float NoV = clamp(dot(N, V), kMinCosine, 1);
    float variance = SlopeVariance(waves.moments);
    float envRoughness = PerceptualRoughness(lerp(EnvironmentRoughnessParameter(), kFoamRoughness, foamAlbedo.a),
                                             variance);
    float sunRoughness = PerceptualRoughness(lerp(SunRoughnessParameter(), kFoamRoughness, foamAlbedo.a), variance);

    Refraction refraction = RefractScene(w, waves.moments.xy);
    float3 transmittance = Transmittance(refraction.pathDepth);
    float3 inscatter = InScattering(V, NoV, CrestTilt(waves.moments), waves.foam.y, transmittance);
    float3 water = transmittance * refraction.scene + inscatter;
    float3 foam = FoamLight(N, waves.foam.x) * foamAlbedo.rgb + inscatter;
    float3 colour = lerp(water, foam, foamAlbedo.a);

    float3 R = ReflectedDirection(V, N);
    float4 screenReflection = TraceScreenReflection(w, pixel, R);
    float3 reflected = lerp(SkyColour(R.z), screenReflection.rgb, screenReflection.a);
    float3 environment = reflected * EnvironmentBrdf(envRoughness, NoV) * ReflectionStrength();
    float3 H = normalize(V + L);
    float environmentAttenuation = 1 - FresnelSchlick(dot(H, V)) * SunVisibility();
    float3 sunSpecular = SunVisibility() * SunColour() * SpecularStrength() * SunGlint(sunRoughness, N, V, L);
    colour += sunSpecular + environmentAttenuation * environment;
    colour = lerp(refraction.scene, colour, saturate(kShoreFadePerYard * refraction.pathDepth));

    [branch] if (WaterDebugView() > 0.5)
        return float4(DebugColour(N, foamAlbedo, transmittance, reflected), 1);
    float3 encoded = LinearToGamma(colour);
    [branch] if (StockFogApplies())
        encoded = lerp(StockFogColour(), encoded, StockFogVisibility(w.waterZ));
    return float4(encoded, 1);
}
