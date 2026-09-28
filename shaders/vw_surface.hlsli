static const float kPi = 3.14159265;
static const float kShallowTileAmplitude = 800;
static const float kMaxTanhArgument = 10;
static const float kAdtGridOrigin = 17066.666;
static const float kAdtTilesPerYard = 0.001875;
static const float kSlopeToAdtUv = 0.001;
static const float kMinFoamFade = 1e-4;

struct WaterPixel
{
    float2 uv;
    float waterZ;
    float sceneZ;
    float3 position;
    float3 toCamera;
    float columnDepth;
};

struct WaveState
{
    float4 moments;
    float2 foam;
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

WaterPixel ReconstructWaterPixel(float2 pixel)
{
    WaterPixel w;
    w.uv = CopyUv(pixel);
    w.waterZ = CopiedViewDepth(sWaterDepth, w.uv);
    w.sceneZ = max(CopiedViewDepth(sSceneDepth, w.uv), w.waterZ);
    float3 worldRay = ViewToWorldDirection(ViewRayAtUnitDepth(pixel));
    w.position = CameraPositionWorld() + worldRay * w.waterZ;
    w.toCamera = normalize(-worldRay);
    w.columnDepth = max(0, (w.sceneZ - w.waterZ) * -worldRay.z);
    return w;
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

WaveState SampleWaves(WaterPixel w)
{
    float4 inverseSize = cInverseTileSizes;
    float4 weight = float4(ShallowTileWeight(inverseSize.x, w.columnDepth),
                           ShallowTileWeight(inverseSize.y, w.columnDepth),
                           ShallowTileWeight(inverseSize.z, w.columnDepth),
                           ShallowTileWeight(inverseSize.w, w.columnDepth));
    float2 xy = w.position.xy;
    WaveState waves;
    waves.moments = weight.x * tex2D(sSurface0, xy * inverseSize.x) + weight.y * tex2D(sSurface1, xy * inverseSize.y) +
                    weight.z * tex2D(sSurface2, xy * inverseSize.z) + weight.w * tex2D(sSurface3, xy * inverseSize.w);
    waves.foam = weight.x * tex2D(sFoamState0, xy * inverseSize.x).xy +
                 weight.y * tex2D(sFoamState1, xy * inverseSize.y).xy +
                 weight.z * tex2D(sFoamState2, xy * inverseSize.z).xy +
                 weight.w * tex2D(sFoamState3, xy * inverseSize.w).xy;
    float scale = WaveScale();
    waves.moments *= float4(scale, scale, scale * scale, scale * scale);
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

float4 FoamLayer(sampler2D mask, int slot, float2 uv)
{
    float coverage = tex2D(mask, uv).r * MaskPresent(slot);
    return float4(lerp(MaskTintLow(slot), MaskTintHigh(slot), coverage), coverage);
}

float FoamFade(float distance, float fadeDistance)
{
    float t = saturate(1 - distance / max(fadeDistance, kMinFoamFade));
    float eased = t * t * (3 - 2 * t);
    eased *= eased;
    return eased * eased;
}

float4 FoamAlbedo(WaterPixel w, WaveState waves)
{
    float2 adtUv = AdtUv(w.position, waves.moments.xy);
    float4 high = FoamLayer(sHighFoamMask, kHighFoamSlot, adtUv * cWaveFoamScaling.x + cFoamScroll.xy);
    float4 mid = FoamLayer(sMidFoamMask, kMidFoamSlot, adtUv * cWaveFoamScaling.y + cFoamScroll.xy);
    float4 low = FoamLayer(sLowFoamMask, kLowFoamSlot, adtUv * cWaveFoamScaling.z + cFoamScroll.xy);
    float4 shore = FoamLayer(sShoreFoamMask, kShoreFoamSlot, adtUv * cShoreFoam.y + cFoamScroll.zw);
    float4 depth = FoamLayer(sDepthFoamMask, kDepthFoamSlot, adtUv * cDepthFadeFoam.y + cDepthFoamScroll.xy);
    float f = max(waves.foam.x, 0);
    float rootF = sqrt(f);
    float4 foam = saturate(low * rootF + mid * f * rootF + high * pow(f, 4.5)) * WaveFoamIntensity();
    foam += shore * cShoreFoam.x * FoamFade(w.columnDepth, cShoreFoam.z);
    foam += depth * cDepthFadeFoam.x * FoamFade(w.sceneZ - w.waterZ, cDepthFadeFoam.z);
    return saturate(foam);
}
