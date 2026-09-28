#include "vf_integrate.hlsli"

float4 cComposite : register(c96);
float4 cGodRayColour : register(c97);
float4 cSun : register(c98);

sampler2D sFog : register(s1);
sampler2D sGodRays : register(s2);
sampler2D sSceneBeforeFog : register(s3);
sampler2D sCurrentMarch : register(s4);

static const float kHighlightKnee = 0.8;
static const float kSunMarkerRadius = 6;
static const float4 kSunMarkerColour = float4(1, 0, 0, 1);
static const float kDebugRadiance = 1;
static const float kDebugTransmittance = 2;
static const float kDebugLinearDepth = 3;
static const float kSameSurfaceAbsoluteDepth = 0.5;
static const float kSameSurfaceRelativeDepth = 0.02;
static const float kTapPlaneRelativeDepth = 0.005;
static const float kWholeBilinearWeight = 0.999;
static const float kMaxDisplayedMarchCurvature = 2.0 / 255;

float Exposure()
{
    return cComposite.x;
}

float GodRayStrength()
{
    return cComposite.y;
}

float DebugView()
{
    return cComposite.z;
}

bool DebugViewEnabled()
{
    return DebugView() > 0.5;
}

bool DebugViewAtMost(float view)
{
    return DebugView() < view + 0.5;
}

float BlendMode()
{
    return cComposite.w;
}

bool BlendsInLinearLight()
{
    return BlendMode() > 0.5 && BlendMode() < 2.5;
}

bool BlendsOverSceneCopy()
{
    return (BlendMode() > 0.5 && BlendMode() < 1.5) || BlendMode() > 2.5;
}

float2 SunPixel()
{
    return cSun.xy;
}

bool SunMarkerEnabled()
{
    return cSun.z > 0;
}

float ClientGlowToCompensate()
{
    return cSun.w;
}

float3 GammaToLinear(float3 colour)
{
    return pow(colour, 2.2);
}

float3 LinearToGamma(float3 colour)
{
    return pow(colour, 1 / 2.2);
}

float3 RollOffHighlights(float3 colour, float3 knee)
{
    float3 span = max(1 - knee, 1e-4);
    return colour <= knee ? colour : knee + span * (1 - exp(-(colour - knee) / span));
}

float3 BeforeClientGlow(float3 onScreen, float glow)
{
    return 2 * onScreen / (1 + sqrt(1 + 4 * glow * onScreen));
}

bool SameDepthClass(float depth, float tapDepth)
{
    return IsSky(depth) == IsSky(tapDepth) && BeyondFarClip(depth) == BeyondFarClip(tapDepth);
}

struct BilinearTaps
{
    float2 baseTexel;
    float2 fraction;
    float4 currentMarch;
    float depth;
    float sameClassWeight;
    float farthestViewZ;
};

BilinearTaps GatherBilinearTaps(float2 pixel, float depth, float viewZ)
{
    float2 lowResCoord = FullPixelToLowResTexel(pixel);
    BilinearTaps taps;
    taps.baseTexel = floor(lowResCoord);
    taps.fraction = lowResCoord - taps.baseTexel;
    taps.currentMarch = 0;
    taps.depth = 0;
    taps.sameClassWeight = 0;
    taps.farthestViewZ = viewZ;
    [loop] for (int j = 0; j < 4; j++)
    {
        float2 tapOffset = float2(frac(j * 0.5) * 2, floor(j * 0.5));
        float2 tapTexel = clamp(taps.baseTexel + tapOffset, 0, LowResSize() - 1);
        float tapDepth = SampleDepth(sDepth, LowResTexelToFullPixel(tapTexel));
        float2 bilinearWeight = lerp(1 - taps.fraction, taps.fraction, tapOffset);
        float weight = bilinearWeight.x * bilinearWeight.y;
        taps.currentMarch += tex2Dlod(sCurrentMarch, float4(LowResTexelToUv(tapTexel), 0, 0)) * weight;
        taps.depth += tapDepth * weight;
        taps.sameClassWeight += SameDepthClass(depth, tapDepth) ? weight : 0;
        taps.farthestViewZ = max(taps.farthestViewZ, weight > 0 ? LinearDepth(tapDepth) : viewZ);
    }
    return taps;
}

bool PixelOnTapPlane(BilinearTaps taps, float viewZ)
{
    return taps.sameClassWeight > kWholeBilinearWeight && taps.farthestViewZ < HorizonBlendStart() &&
           abs(LinearDepth(taps.depth) - viewZ) <= viewZ * kTapPlaneRelativeDepth;
}

bool RayMeetsLocalLight(float2 pixel, float viewZ)
{
    float3 viewRay = ViewRayAtUnitDepth(pixel);
    float distancePerViewZ = length(viewRay);
    float3 viewDirection = viewRay / distancePerViewZ;
    float rayLength = viewZ * distancePerViewZ;
    bool meets = false;
    [loop] for (int index = 0; index < LocalLightCount(); ++index)
    {
        float2 chord = LoadLocalLight(index, viewDirection).chord;
        meets = meets || (chord.y > 0 && chord.x < rayLength);
    }
    return meets;
}

float4 DisplayedCurrentMarch(float2 lowResTexel)
{
    float2 uv = LowResTexelToUv(clamp(lowResTexel, 0, LowResSize() - 1));
    float4 fog = tex2Dlod(sCurrentMarch, float4(uv, 0, 0));
    fog.rgb *= Exposure();
    fog.rgb = BlendsInLinearLight() ? sqrt(max(fog.rgb, 0)) : fog.rgb;
    return fog;
}

float4 DisplayedMarchCurvature(float2 firstTexel, float2 axis)
{
    float4 before = DisplayedCurrentMarch(firstTexel - axis);
    float4 first = DisplayedCurrentMarch(firstTexel);
    float4 second = DisplayedCurrentMarch(firstTexel + axis);
    float4 after = DisplayedCurrentMarch(firstTexel + 2 * axis);
    return max(abs(before - 2 * first + second), abs(first - 2 * second + after));
}

bool MarchLinearBetweenTaps(BilinearTaps taps)
{
    float4 curvature = 0;
    [loop] for (int k = 0; k < 4; k++)
    {
        float2 axis = k < 2 ? float2(0, 1) : float2(1, 0);
        float2 across = 1 - axis;
        bool secondLine = k == 1 || k == 3;
        bool interpolated = dot(taps.fraction, axis) > 0 && (!secondLine || dot(taps.fraction, across) > 0);
        float2 firstTexel = taps.baseTexel + (secondLine ? across : float2(0, 0));
        [branch] if (interpolated)
            curvature = max(curvature, DisplayedMarchCurvature(firstTexel, axis));
    }
    return max(max(curvature.r, curvature.g), max(curvature.b, curvature.a)) <= kMaxDisplayedMarchCurvature;
}

bool TapsReproduceMarch(BilinearTaps taps, float viewZ)
{
    [branch] if (!PixelOnTapPlane(taps, viewZ))
        return false;
    return MarchLinearBetweenTaps(taps);
}

float4 FogWithoutMatchingTap(float2 pixel, float depth, float viewZ)
{
    [branch] if (!kMarchesLocalLights || !RayMeetsLocalLight(pixel, viewZ))
    {
        BilinearTaps taps = GatherBilinearTaps(pixel, depth, viewZ);
        [branch] if (TapsReproduceMarch(taps, viewZ))
            return taps.currentMarch;
    }
    return IntegrateFogAtPixel(pixel, 0.5);
}

float4 DepthAwareUpsample(float2 pixel, float depth, float viewZ)
{
    float2 lowResCoord = FullPixelToLowResTexel(pixel);
    float2 baseTexel = floor(lowResCoord);
    float2 bilinearFraction = lowResCoord - baseTexel;

    float4 weightedFog = 0;
    float weightSum = 0;
    [loop] for (int j = 0; j < 4; j++)
    {
        float2 tapOffset = float2(frac(j * 0.5) * 2, floor(j * 0.5));
        float2 tapTexel = clamp(baseTexel + tapOffset, 0, LowResSize() - 1);
        float tapDepth = SampleDepth(sDepth, LowResTexelToFullPixel(tapTexel));
        float tapViewZ = LinearDepth(tapDepth);
        float2 bilinearWeight = lerp(1 - bilinearFraction, bilinearFraction, tapOffset);
        bool sameClass = SameDepthClass(depth, tapDepth);
        bool sameDepth = abs(tapViewZ - viewZ) <= max(kSameSurfaceAbsoluteDepth, viewZ * kSameSurfaceRelativeDepth);
        float relativeDepthDifference = abs(tapViewZ - viewZ) / max(viewZ, 1e-3);
        float weight = sameClass && sameDepth ?
            bilinearWeight.x * bilinearWeight.y / (1e-3 + relativeDepthDifference) : 0;
        weightedFog += tex2Dlod(sFog, float4(LowResTexelToUv(tapTexel), 0, 0)) * weight;
        weightSum += weight;
    }
    [branch] if (weightSum > 1e-6)
        return weightedFog / weightSum;
    return FogWithoutMatchingTap(pixel, depth, viewZ);
}

float3 DisplaySpaceGodRays(float2 viewportUv)
{
    float3 godRays = 0;
    [branch] if (GodRayStrength() > 0)
        godRays = tex2Dlod(sGodRays, float4(viewportUv, 0, 0)).rgb * cGodRayColour.rgb * GodRayStrength();
    return godRays;
}

float3 BlendGodRays(float3 colour, float3 godRays)
{
    [branch] if (GodRayStrength() > 0)
    {
        float glow = ClientGlowToCompensate();
        float3 displayed = colour * (1 + glow * colour);
        displayed += max(1 - displayed, 0) * (1 - exp(-max(godRays, 0)));
        colour = BeforeClientGlow(displayed, glow);
    }
    return colour;
}

float3 FogSceneColour(float3 scene, float4 fog)
{
    [branch] if (!BlendsInLinearLight())
    {
        float3 fogColour = RollOffHighlights(fog.rgb / max(fog.a, 1e-4), kHighlightKnee);
        float3 colour = scene * (1 - fog.a) + fogColour * fog.a;
        return colour;
    }
    scene = GammaToLinear(scene);
    float3 colour = LinearToGamma(RollOffHighlights(scene * (1 - fog.a) + fog.rgb, max(kHighlightKnee, scene)));
    [branch] if (ClientGlowToCompensate() > 0)
        colour = lerp(colour, BeforeClientGlow(colour, ClientGlowToCompensate()), fog.a);
    return colour;
}

float4 BlendOverSceneCopy(float2 viewportUv, float4 fog, float3 godRays)
{
    float3 scene = tex2Dlod(sSceneBeforeFog, float4(viewportUv, 0, 0)).rgb;
    return float4(BlendGodRays(FogSceneColour(scene, fog), godRays), 1);
}

float4 PremultipliedForFixedFunctionBlend(float4 fog, float3 godRays, bool linearLight)
{
    float opacity = max(fog.a, 1e-4);
    float3 unpremultiplied = RollOffHighlights(fog.rgb / opacity, kHighlightKnee);
    if (linearLight)
        unpremultiplied = LinearToGamma(unpremultiplied);
    return float4(unpremultiplied * fog.a + godRays, fog.a);
}

float4 main(float2 pixelIndex : VPOS) : COLOR0
{
    float2 pixel = pixelIndex + 0.5;
    float depth = SampleDepth(sDepth, pixel);
    float viewZ = LinearDepth(depth);
    float4 fog = DepthAwareUpsample(pixel, depth, viewZ);
    fog.rgb *= Exposure();

    float2 viewportUv = (pixel - ViewportOrigin()) / ViewportSize();
    float3 godRays = DisplaySpaceGodRays(viewportUv);
    const bool linearLight = BlendsInLinearLight();

    [branch] if (DebugViewEnabled())
    {
        if (DebugViewAtMost(kDebugRadiance))
            return float4((linearLight ? LinearToGamma(saturate(fog.rgb)) : fog.rgb) + godRays, 1);
        if (DebugViewAtMost(kDebugTransmittance))
            return float4(1 - fog.aaa, 1);
        if (DebugViewAtMost(kDebugLinearDepth))
            return float4(saturate(viewZ / MaxFogDistance()).xxx, 1);
    }
    [branch] if (SunMarkerEnabled() && distance(pixel, SunPixel()) < kSunMarkerRadius)
        return kSunMarkerColour;

    [branch] if (BlendsOverSceneCopy())
        return BlendOverSceneCopy(viewportUv, fog, godRays);
    return PremultipliedForFixedFunctionBlend(fog, godRays, linearLight);
}
