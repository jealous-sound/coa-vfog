static const float kPi = 3.14159265;
static const float kShallowTileAmplitude = 800;
static const float kMaxTanhArgument = 10;
static const float kAdtGridOrigin = 17066.666;
static const float kAdtTilesPerYard = 0.001875;
static const float kSlopeToAdtUv = 0.001;
static const float kMinFoamFade = 1e-4;
static const float kMinRayRise = 1e-4;
static const float kRippleSlopeScale = 3;
static const float kRippleFullDetailTexels = 2;
static const float kRippleDetailFadeTexels = 2;

struct WaterPixel
{
    float2 uv;
    float waterZ;
    float sceneZ;
    float3 position;
    float3 toCamera;
    float cameraDistance;
    float columnDepth;
    float2 footprintX;
    float2 footprintY;
};

struct WaveState
{
    float4 moments;
    float2 foam;
};

struct RippleSample
{
    float height;
    float2 slope;
    float coverage;
};

float2 CopyUv(float2 pixel)
{
    return (pixel - ViewportOrigin()) / ViewportSize();
}

float CopiedViewDepth(sampler2D copy, float2 uv)
{
    return dot(tex2Dlod(copy, float4(uv, 0, 0)).rgb, cDepthDecode.rgb);
}

float3 GammaToLinear(float3 colour)
{
    return pow(max(colour, 0), 2.2);
}

float3 LinearToGamma(float3 colour)
{
    return pow(saturate(colour), 1 / 2.2);
}

float2 PlaneFootprint(float3 ray, float3 rayStep, float viewZ)
{
    float rise = ray.z < 0 ? min(ray.z, -kMinRayRise) : max(ray.z, kMinRayRise);
    return viewZ * (rayStep.xy - ray.xy * rayStep.z / rise);
}

WaterPixel ReconstructWaterPixel(float2 pixel)
{
    WaterPixel w;
    w.uv = CopyUv(pixel);
    w.waterZ = CopiedViewDepth(sWaterDepth, w.uv);
    w.sceneZ = max(CopiedViewDepth(sSceneDepth, w.uv), w.waterZ);
    float3 worldRay = ViewToWorldDirection(ViewRayAtUnitDepth(pixel));
    w.position = CameraPositionWorld() + worldRay * w.waterZ;
    w.toCamera = normalize(-worldRay);
    w.cameraDistance = w.waterZ * length(worldRay);
    w.columnDepth = max(0, (w.sceneZ - w.waterZ) * -worldRay.z);
    float3 rayStepX = ViewToWorldDirection(ViewRayAtUnitDepth(pixel + float2(1, 0))) - worldRay;
    float3 rayStepY = ViewToWorldDirection(ViewRayAtUnitDepth(pixel + float2(0, 1))) - worldRay;
    w.footprintX = PlaneFootprint(worldRay, rayStepX, w.waterZ);
    w.footprintY = PlaneFootprint(worldRay, rayStepY, w.waterZ);
    return w;
}

float4 SampleTile(sampler2D tile, WaterPixel w, float inverseSize)
{
    return tex2Dgrad(tile, w.position.xy * inverseSize, w.footprintX * inverseSize, w.footprintY * inverseSize);
}

float2 SampleFoamState(sampler2D state, WaterPixel w, float inverseSize)
{
    return tex2Dlod(state, float4(w.position.xy * inverseSize, 0, 0)).xy;
}

float Tanh(float x)
{
    float e = exp(2 * min(x, kMaxTanhArgument));
    return (e - 1) / (e + 1);
}

float ShallowTileWeight(float inverseTileSize, float columnDepth)
{
    float shallowAmplitude = kShallowTileAmplitude * inverseTileSize * inverseTileSize;
    return saturate(lerp(shallowAmplitude, 1, Tanh(4 * kPi * columnDepth * inverseTileSize)));
}

float RippleHeight(float2 uv)
{
    float2 state = tex2Dlod(sRipples, float4(uv, 0, 0)).rg;
    return lerp(state.g, state.r, RippleWeight());
}

float RippleCoverage(WaterPixel w, float2 uv)
{
    float2 fromCentre = abs(uv - 0.5) * RippleExtent();
    float window = saturate((RippleFadeEnd() - max(fromCentre.x, fromCentre.y)) * RippleInverseFadeWidth());
    float footprintTexels = max(length(w.footprintX), length(w.footprintY)) / RippleTexelYards();
    return window * saturate(1 - (footprintTexels - kRippleFullDetailTexels) / kRippleDetailFadeTexels);
}

RippleSample SampleRipples(WaterPixel w)
{
    RippleSample ripples = (RippleSample)0;
    float2 uv = (w.position.xy - RippleOrigin()) * RippleInverseExtent();
    ripples.coverage = RippleGain() > 0 ? RippleCoverage(w, uv) : 0;
    [branch] if (ripples.coverage > 0)
    {
        float step = RippleTexelUv();
        ripples.height = RippleHeight(uv);
        float dx = RippleHeight(uv + float2(step, 0)) - ripples.height;
        float dy = RippleHeight(uv + float2(0, step)) - ripples.height;
        float2 gradient = float2(dx, dy) * rsqrt((1 + dx * dx) * (1 + dy * dy));
        ripples.slope = gradient * kRippleSlopeScale * RippleGain() * ripples.coverage;
    }
    return ripples;
}

WaveState SampleWaves(WaterPixel w, float2 rippleSlope)
{
    float4 inverseSize = cInverseTileSizes;
    float4 weight = float4(ShallowTileWeight(inverseSize.x, w.columnDepth),
                           ShallowTileWeight(inverseSize.y, w.columnDepth),
                           ShallowTileWeight(inverseSize.z, w.columnDepth),
                           ShallowTileWeight(inverseSize.w, w.columnDepth));
    WaveState waves;
    waves.moments = weight.x * SampleTile(sSurface0, w, inverseSize.x) +
                    weight.y * SampleTile(sSurface1, w, inverseSize.y) +
                    weight.z * SampleTile(sSurface2, w, inverseSize.z) +
                    weight.w * SampleTile(sSurface3, w, inverseSize.w);
    waves.moments.xy += rippleSlope;
    waves.foam = weight.x * SampleFoamState(sFoamState0, w, inverseSize.x) +
                 weight.y * SampleFoamState(sFoamState1, w, inverseSize.y) +
                 weight.z * SampleFoamState(sFoamState2, w, inverseSize.z) +
                 weight.w * SampleFoamState(sFoamState3, w, inverseSize.w);
    return waves;
}

float SlopeVariance(float4 moments)
{
    float2 meanSlope = moments.xy * InverseTileCount();
    return 0.5 * max(0, moments.z * InverseTileCount() - dot(meanSlope, meanSlope));
}

float CrestTilt(float4 moments)
{
    float coarseSlope = moments.w * InverseTileCount();
    return sqrt(coarseSlope / (coarseSlope + 1));
}

float2 AdtUv(float3 position, float2 slope)
{
    return (kAdtGridOrigin - position.yx) * kAdtTilesPerYard + slope * kSlopeToAdtUv;
}

float4 TintedFoam(int slot, float maskCoverage)
{
    float coverage = maskCoverage * MaskPresent(slot);
    return float4(lerp(MaskTintLow(slot), MaskTintHigh(slot), coverage), coverage);
}

float4 FoamLayer(sampler2D mask, int slot, float2 uv, float2 dx, float2 dy)
{
    return TintedFoam(slot, tex2Dgrad(mask, uv, dx, dy).r);
}

float FoamFade(float distance, float fadeDistance)
{
    float t = saturate(1 - distance / max(fadeDistance, kMinFoamFade));
    float eased = t * t * (3 - 2 * t);
    eased *= eased;
    return eased * eased;
}

float4 WaveFoam(float2 adtUv, float2 dx, float2 dy, float f)
{
    float3 scale = cWaveFoamScaling.xyz;
    float4 high = TintedFoam(kHighFoamSlot, tex2Dgrad(sWaveFoamMasks, adtUv * scale.x + cFoamScroll.xy, dx * scale.x,
                                                      dy * scale.x).r);
    float4 mid = TintedFoam(kMidFoamSlot, tex2Dgrad(sWaveFoamMasks, adtUv * scale.y + cFoamScroll.xy, dx * scale.y,
                                                    dy * scale.y).g);
    float4 low = TintedFoam(kLowFoamSlot, tex2Dgrad(sWaveFoamMasks, adtUv * scale.z + cFoamScroll.xy, dx * scale.z,
                                                    dy * scale.z).b);
    float rootF = sqrt(f);
    return saturate(low * rootF + mid * f * rootF + high * pow(f, 4.5)) * WaveFoamIntensity();
}

float4 FoamAlbedo(WaterPixel w, WaveState waves)
{
    float2 adtUv = AdtUv(w.position, waves.moments.xy);
    float2 dx = -w.footprintX.yx * kAdtTilesPerYard;
    float2 dy = -w.footprintY.yx * kAdtTilesPerYard;
    float f = max(waves.foam.x, 0);
    float shoreWeight = cShoreFoam.x * FoamFade(w.columnDepth * ShoreDistancePerDepth(), cShoreFoam.z);
    float depthWeight = cDepthFadeFoam.x * FoamFade(w.sceneZ - w.waterZ, cDepthFadeFoam.z);
    float4 foam = 0;
    [branch] if (f * WaveFoamIntensity() > 0)
        foam = WaveFoam(adtUv, dx, dy, f);
    [branch] if (shoreWeight > 0)
        foam += shoreWeight * FoamLayer(sShoreFoamMask, kShoreFoamSlot, adtUv * cShoreFoam.y + cFoamScroll.zw,
                                        dx * cShoreFoam.y, dy * cShoreFoam.y);
    [branch] if (depthWeight > 0)
        foam += depthWeight * FoamLayer(sDepthFoamMask, kDepthFoamSlot, adtUv * cDepthFadeFoam.y + cDepthFoamScroll.xy,
                                        dx * cDepthFadeFoam.y, dy * cDepthFadeFoam.y);
    return saturate(foam);
}
