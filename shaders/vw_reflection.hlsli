static const float kHorizonReflectionLift = 1e-4;
static const float kSkyLowerBandElevation = 0.06;
static const float kSkyUpperBandElevation = 0.15;
static const float kSkyMiddleElevation = 0.35;
static const float kSkyTopElevation = 0.8;

float3 ReflectedDirection(float3 V, float3 N)
{
    float3 r = reflect(-V, N);
    return r.z < 0 ? normalize(float3(r.xy, kHorizonReflectionLift)) : r;
}

float3 SkyColour(float elevation)
{
    float3 colour = lerp(cSkyHorizon.rgb, cSkyLowerBand.rgb, smoothstep(0, kSkyLowerBandElevation, elevation));
    colour = lerp(colour, cSkyUpperBand.rgb, smoothstep(kSkyLowerBandElevation, kSkyUpperBandElevation, elevation));
    colour = lerp(colour, cSkyMiddle.rgb, smoothstep(kSkyUpperBandElevation, kSkyMiddleElevation, elevation));
    return lerp(colour, cSkyTop.rgb, smoothstep(kSkyMiddleElevation, kSkyTopElevation, elevation));
}

#if SSR_STEPS > 0
static const int kReflectionSteps = SSR_STEPS;
static const int kReflectionRefinements = 4;
static const float kMinReflectionViewZ = 1e-3;
static const float kMinInverseViewZ = 1e-8;
static const float kMinScreenStep = 1e-6;
static const float kReflectionThicknessYards = 1.5;
static const float kReflectionRelativeThickness = 0.05;
static const float kVerticalFadeStart = 0.9;
static const float kVerticalFadeRate = 10;
static const float kNoReflectionHit = 2;

struct ReflectionRay
{
    float2 startPixel;
    float2 vanishingPixel;
    float startInverseZ;
};

float3 WorldToViewDirection(float3 direction)
{
    return float3(dot(direction, cViewToWorld[0].xyz), dot(direction, cViewToWorld[1].xyz),
                  dot(direction, cViewToWorld[2].xyz));
}

float2 ViewDirectionToPixel(float3 viewDirection)
{
    return NdcToPixel(viewDirection.xy / viewDirection.z * ViewToNdcScale() + ViewToNdcOffset());
}

float RayViewZ(ReflectionRay ray, float s)
{
    return 1 / max(ray.startInverseZ * (1 - s), kMinInverseViewZ);
}

float2 RayUv(ReflectionRay ray, float s)
{
    return CopyUv(lerp(ray.startPixel, ray.vanishingPixel, s));
}

float OnScreenExtent(ReflectionRay ray)
{
    float rise = ray.vanishingPixel.y - ray.startPixel.y;
    float top = ViewportOrigin().y;
    float bottom = top + ViewportSize().y;
    float limit = rise < 0 ? (top - ray.startPixel.y) / rise : (bottom - ray.startPixel.y) / max(rise, kMinScreenStep);
    return saturate(limit);
}

bool RayBehindScene(ReflectionRay ray, float s)
{
    return RayViewZ(ray, s) > CopiedViewDepth(sSceneDepth, RayUv(ray, s));
}

float4 TraceScreenReflection(WaterPixel w, float2 pixel, float3 R)
{
    float3 viewDirection = WorldToViewDirection(R);
    [branch] if (viewDirection.z < kMinReflectionViewZ)
        return 0;
    ReflectionRay ray;
    ray.startPixel = pixel;
    ray.vanishingPixel = ViewDirectionToPixel(viewDirection);
    ray.startInverseZ = 1 / w.waterZ;
    float extent = OnScreenExtent(ray);
    float previous = 0;
    float front = 0;
    float behind = kNoReflectionHit;
    [loop] for (int step = 1; step <= kReflectionSteps; ++step)
    {
        [branch] if (behind == kNoReflectionHit)
        {
            float s = extent * step / kReflectionSteps;
            float sceneZ = CopiedViewDepth(sSceneDepth, RayUv(ray, s));
            float thickness = max(kReflectionThicknessYards, sceneZ * kReflectionRelativeThickness);
            bool crossed = RayViewZ(ray, s) > sceneZ && RayViewZ(ray, previous) < sceneZ + thickness;
            front = crossed ? previous : front;
            behind = crossed ? s : behind;
            previous = s;
        }
    }
    [branch] if (behind == kNoReflectionHit)
        return 0;
    [loop] for (int refinement = 0; refinement < kReflectionRefinements; ++refinement)
    {
        float middle = 0.5 * (front + behind);
        bool middleBehind = RayBehindScene(ray, middle);
        front = middleBehind ? front : middle;
        behind = middleBehind ? middle : behind;
    }
    float2 uv = RayUv(ray, behind);
    float fade = saturate(1 - saturate((max(uv.y, 1 - uv.y) - kVerticalFadeStart) * kVerticalFadeRate));
    return float4(GammaToLinear(tex2Dlod(sSceneColour, float4(uv, 0, 0)).rgb), fade);
}
#else
float4 TraceScreenReflection(WaterPixel w, float2 pixel, float3 R)
{
    return 0;
}
#endif
