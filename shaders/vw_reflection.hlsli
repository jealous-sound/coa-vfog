static const float kSkyLowerBandElevation = 0.06;
static const float kSkyUpperBandElevation = 0.15;
static const float kSkyMiddleElevation = 0.35;
static const float kSkyTopElevation = 0.8;

float3 ReflectedDirection(float3 V, float3 N)
{
    float3 r = reflect(-V, N);
    return float3(r.xy, abs(r.z));
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

float4 TraceScreenReflection(WaterPixel w, float2 pixel, float3 R, out float2 hitUv)
{
    hitUv = 0;
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
    hitUv = uv;
    float fade = saturate(1 - saturate((max(uv.y, 1 - uv.y) - kVerticalFadeStart) * kVerticalFadeRate));
    return float4(GammaToLinear(tex2Dlod(sSceneColour, float4(uv, 0, 0)).rgb), fade);
}
#else
float4 TraceScreenReflection(WaterPixel w, float2 pixel, float3 R, out float2 hitUv)
{
    hitUv = 0;
    return 0;
}
#endif

static const int kReflectionFogLayers = 4;
static const int kReflectionFogLayerRegisters = 5;
static const float kSkyHitDepthFraction = 0.99;
static const float kMinPhaseDistance = 1e-6;

float ReflectionFogCurveIntegral(float4 curve, float distance)
{
    float travelled = max(distance - curve.x, 0);
    float range = ReflectionFogCurveRange();
    float ramp = saturate(travelled / range);
    float exponent = curve.w + 1;
    return travelled + curve.z * (range * pow(ramp, exponent) / exponent + max(travelled - range, 0));
}

float ReflectionFogHeightProfile(float4 height, float worldHeight)
{
    return saturate(exp((height.x - worldHeight) * height.y)) * saturate(exp((worldHeight - height.z) * height.w));
}

float ReflectionFogPhase(float4 scattering, float cosToLight)
{
    float g = scattering.x;
    float r = (1 - g) / sqrt(max(1 + g * g - 2 * g * cosToLight, kMinPhaseDistance));
    return lerp(r * r * r, 1, scattering.y);
}

float4 ReflectedPathFog(float from, float to, float meanHeight, float skyRise, float cosToLight)
{
    float3 inscatter = 0;
    float opticalDepth = 0;
    [unroll] for (int j = 0; j < kReflectionFogLayers; ++j)
    {
        float4 curve = cReflectionFogLayers[j * kReflectionFogLayerRegisters];
        float4 height = cReflectionFogLayers[j * kReflectionFogLayerRegisters + 1];
        float3 emissive = cReflectionFogLayers[j * kReflectionFogLayerRegisters + 2].rgb;
        float3 diffuse = cReflectionFogLayers[j * kReflectionFogLayerRegisters + 3].rgb;
        float4 scattering = cReflectionFogLayers[j * kReflectionFogLayerRegisters + 4];
        [branch] if (curve.y <= 0)
            continue;
        float start = max(from, curve.x);
        float end = min(to, scattering.z);
        float span = end > start ? ReflectionFogCurveIntegral(curve, end) - ReflectionFogCurveIntegral(curve, start)
                                 : 0;
        float layerDepth = curve.y * span * ReflectionFogHeightProfile(height, meanHeight) *
                           exp(-skyRise * scattering.w);
        inscatter += (diffuse * ReflectionFogPhase(scattering, cosToLight) + emissive) * layerDepth;
        opticalDepth += layerDepth;
    }
    float transmittance = exp(-opticalDepth);
    return float4(opticalDepth > 0 ? inscatter / opticalDepth * (1 - transmittance) : 0, transmittance);
}

float3 FoggedSkyReflection(WaterPixel w, float3 R, float3 sky)
{
    [branch] if (!ReflectionFogActive())
        return sky;
    float rise = max(R.z, 0);
    float to = max(ReflectionFogSkyEnd(), w.cameraDistance);
    float meanHeight = w.position.z + 0.5 * rise * (to - w.cameraDistance);
    float4 fog = ReflectedPathFog(w.cameraDistance, to, meanHeight, rise, dot(R, ToLight()));
    return sky * fog.a + fog.rgb;
}

float3 FoggedScreenReflection(WaterPixel w, float3 R, float3 colour, float2 hitUv)
{
    [branch] if (!ReflectionFogActive())
        return colour;
    float hitZ = CopiedViewDepth(sSceneDepth, hitUv);
    [branch] if (hitZ >= kSkyHitDepthFraction * MaxFogDistance())
        return FoggedSkyReflection(w, R, colour);
    float2 hitPixel = ViewportOrigin() + hitUv * ViewportSize();
    float3 hit = CameraPositionWorld() + ViewToWorldDirection(ViewRayAtUnitDepth(hitPixel)) * hitZ;
    float to = w.cameraDistance + distance(hit, w.position);
    float4 fog = ReflectedPathFog(w.cameraDistance, to, 0.5 * (w.position.z + hit.z), 0, dot(R, ToLight()));
    return colour * fog.a + fog.rgb;
}
