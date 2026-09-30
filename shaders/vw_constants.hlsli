float4 cLight : register(c9);
float4 cSunColour : register(c10);
float4 cAmbient : register(c11);
float4 cIsotropicLight : register(c12);
float4 cSkyTop : register(c13);
float4 cSkyMiddle : register(c14);
float4 cSkyUpperBand : register(c15);
float4 cSkyLowerBand : register(c16);
float4 cSkyHorizon : register(c17);
float4 cStockFogColour : register(c18);
float4 cStockFog : register(c19);
float4 cAbsorption : register(c20);
float4 cScatteringIntensities : register(c21);
float4 cScatteringTop : register(c22);
float4 cScatteringBottom : register(c23);
float4 cDepthFadeFoam : register(c24);
float4 cShoreFoam : register(c25);
float4 cWaveFoam : register(c26);
float4 cWaveFoamScaling : register(c27);
float4 cSurfaceResponse : register(c28);
float4 cInverseTileSizes : register(c29);
float4 cWaveControl : register(c30);
float4 cFoamScroll : register(c31);
float4 cDepthFoamScroll : register(c32);
float4 cDepthDecode : register(c33);
float4 cMaskTints[10] : register(c34);
float4 cReflectionFogLayers[20] : register(c44);
float4 cReflectionFogRange : register(c64);
float4 cRippleWindow : register(c65);
float4 cRippleShape : register(c66);
float4 cRippleFade : register(c67);

sampler2D sSceneColour : register(s0);
sampler2D sSceneDepth : register(s1);
sampler2D sWaterDepth : register(s2);
sampler2D sSurface0 : register(s3);
sampler2D sSurface1 : register(s4);
sampler2D sSurface2 : register(s5);
sampler2D sSurface3 : register(s6);
sampler2D sFoamState0 : register(s7);
sampler2D sFoamState1 : register(s8);
sampler2D sFoamState2 : register(s9);
sampler2D sFoamState3 : register(s10);
sampler2D sWaveFoamMasks : register(s11);
sampler2D sRipples : register(s12);
sampler2D sShoreFoamMask : register(s14);
sampler2D sDepthFoamMask : register(s15);

static const int kHighFoamSlot = 0;
static const int kMidFoamSlot = 1;
static const int kLowFoamSlot = 2;
static const int kShoreFoamSlot = 3;
static const int kDepthFoamSlot = 4;

float3 ToLight()
{
    return cLight.xyz;
}

float SunVisibility()
{
    return cLight.w;
}

float3 SunColour()
{
    return cSunColour.rgb;
}

float SunTransmission()
{
    return cSunColour.w;
}

float3 Ambient()
{
    return cAmbient.rgb;
}

float3 IsotropicLight()
{
    return cIsotropicLight.rgb;
}

bool StockFogApplies()
{
    return cStockFogColour.w > 0.5;
}

float3 StockFogColour()
{
    return cStockFogColour.rgb;
}

float StockFogVisibility(float viewZ)
{
    return saturate((cStockFog.x - viewZ) * cStockFog.y);
}

static const float kMinStockFogVisibility = 0.05;

float3 WithoutStockFog(float3 encoded, float viewZ)
{
    [branch] if (!StockFogApplies())
        return encoded;
    float visibility = StockFogVisibility(viewZ);
    return saturate((encoded - StockFogColour() * (1 - visibility)) / max(visibility, kMinStockFogVisibility));
}

float3 AbsorptionPerYard()
{
    return cAbsorption.rgb;
}

float3 CrestScatteringColour()
{
    return cScatteringTop.rgb;
}

float3 TroughScatteringColour()
{
    return cScatteringBottom.rgb;
}

float ScatteringAnisotropy()
{
    return cScatteringBottom.w;
}

float WaveFoamIntensity()
{
    return cWaveFoam.x;
}

float ShoreDistancePerDepth()
{
    return cShoreFoam.w;
}

float SunRoughnessParameter()
{
    return cSurfaceResponse.x;
}

float EnvironmentRoughnessParameter()
{
    return cSurfaceResponse.y;
}

float ReflectionStrength()
{
    return cSurfaceResponse.z;
}

float SpecularStrength()
{
    return cSurfaceResponse.w;
}

float InverseTileCount()
{
    return cWaveControl.x;
}

float WaterClassIndex()
{
    return cWaveControl.z;
}

float WaterDebugView()
{
    return cWaveControl.w;
}

float3 MaskTintLow(int slot)
{
    return cMaskTints[slot * 2].rgb;
}

float MaskPresent(int slot)
{
    return cMaskTints[slot * 2].w;
}

bool ReflectionFogActive()
{
    return cReflectionFogRange.w > 0.5;
}

float ReflectionFogCurveRange()
{
    return cReflectionFogRange.x;
}

float ReflectionFogSkyEnd()
{
    return cReflectionFogRange.y;
}

float3 MaskTintHigh(int slot)
{
    return cMaskTints[slot * 2 + 1].rgb;
}

float2 RippleOrigin()
{
    return cRippleWindow.xy;
}

float RippleInverseExtent()
{
    return cRippleWindow.z;
}

float RippleTexelUv()
{
    return cRippleWindow.w;
}

float RippleWeight()
{
    return cRippleShape.x;
}

float RippleGain()
{
    return cRippleShape.y;
}

float RippleExtent()
{
    return cRippleShape.z;
}

float RippleFadeEnd()
{
    return cRippleFade.x;
}

float RippleInverseFadeWidth()
{
    return cRippleFade.y;
}

float RippleTexelYards()
{
    return cRippleFade.z;
}
