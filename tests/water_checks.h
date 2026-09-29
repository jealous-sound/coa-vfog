#pragma once

namespace water_checks
{
constexpr float kWaterSurfaceZ = 0.0f;
constexpr float kBasinFloorZ = -3.0f;
constexpr float kLandZ = 0.5f;
constexpr float kBasinNearX = 30.0f;
constexpr float kBeachStartX = 300.0f;
constexpr float kBasinFarX = 420.0f;
constexpr float kBasinHalfWidth = 260.0f;
constexpr float kLandExtent = 2000.0f;
constexpr DWORD kLandColour = 0xFF4F6B3A;
constexpr DWORD kFloorColour = 0xFFA89060;
constexpr DWORD kWallColour = 0xFF7A6A50;
constexpr DWORD kBoxColour = 0xFF6E5A48;
constexpr DWORD kFarWallColour = 0xFF5A5A62;
constexpr DWORD kStockWaterColour = 0x8030507A;
constexpr DWORD kInvisibleWaterColour = 0x0030507A;
constexpr DWORD kWaterMaskColour = 0xFFFF00FF;
constexpr DWORD kOtherPassColour = 0xFF20C0C0;
constexpr DWORD kCoverLiquidColour = 0xFFC03030;
constexpr float kCoverLiquidZ = 0.3f;
constexpr float kOtherPassRawDepth = 0.05f;
constexpr float kOtherPassLeft = 40.0f;
constexpr float kOtherPassTop = 600.0f;
constexpr float kOtherPassRight = 200.0f;
constexpr float kOtherPassBottom = 680.0f;
constexpr double kFrameSeconds = 12.5;
constexpr double kRealTime = -1.0;
constexpr int kMaskSide = 64;
constexpr DWORD kSamplerStages = 16;
constexpr UINT kSentinelPixelConstants = 99;
constexpr int kLakeClass = static_cast<int>(WaterClass::Lake);
constexpr int kRiverClass = static_cast<int>(WaterClass::River);
constexpr int kOceanClass = static_cast<int>(WaterClass::Ocean);
constexpr float kReferenceTolerance = 2.5f / 255.0f;
constexpr float kDepthViewRange = 255.0f;
constexpr float kMinShadedFraction = 0.9f;
constexpr float kClassProbeX = 250.0f;
constexpr int kInteriorClass = static_cast<int>(WaterClass::Interior);
constexpr float kNearStockFogStart = 5.0f;
constexpr float kNearStockFogEnd = 20.0f;
constexpr uint32_t kStockFogColour = 0xFFC08040;
constexpr uint32_t kShoreStockFogColour = 0xFF40A0FF;
constexpr float kShoreStockFogStart = 0.0f;
constexpr float kShoreStockFogEnd = 300.0f;
constexpr float kFoggedCopyTolerance = 3.0f / 255.0f;
constexpr uint32_t kOtherSunColour = 0xFF4060FF;
constexpr uint32_t kSunsetDirectColour = 0xFFFF7400;
constexpr uint32_t kSunsetSpriteColour = 0xFFFFE7B6;
constexpr float kSunsetElevation = 0.05f;
constexpr float kLitGroundTolerance = 2.0f / 255.0f;
constexpr Vec3 kGrazingEye = {0, 0, 4};
constexpr Vec3 kGrazingTarget = {100, 5, 1};
constexpr int kReflectionDebugView = 4;
constexpr UINT kMirroredShoreRows = 8;
constexpr int kShoreColourTolerance = 3;
constexpr float kMaxShoreReflectionFraction = 0.02f;
constexpr float kRoughTileAmplitude = 30000.0f;
constexpr float kRoughTileWindMultiplier = 3.0f;
constexpr float kFoamEverywhereRange = 10000.0f;
constexpr float kDimFoamTint = 0.25f;
constexpr float kFoamTintTolerance = 0.03f;
constexpr float kMinDecodedFoam = 1e-3f;
constexpr DWORD kClientStencilRef = 7;
constexpr DWORD kClientStencilMask = 0x3C;
constexpr DWORD kClientStencilWriteMask = 0x5A;
constexpr DWORD kClientAlphaRef = 0x20;
constexpr DWORD kClientColourWrite = 0x7;
constexpr DWORD kClientBorderColour = 0x11223344;
constexpr DWORD kClientMaxAnisotropy = 2;
constexpr DWORD kStockWaterAlphaRef = 1;
constexpr UINT kClientStreamOffset = 16;
constexpr UINT kClientStreamStride = 16;
constexpr RECT kClientScissor = {3, 5, 700, 400};
constexpr int kNoWaterFault = 0;
constexpr int kFaultInBegin = 1;
constexpr int kFaultInEnd = 2;
constexpr int kLiquidRendererWords = 8;
std::wstring g_harnessOutDir;
constexpr int kTimedWaterFrames = 30;
constexpr unsigned kAllWaterResources = 0x7;
constexpr int kFoamClockSteps = 20;
constexpr double kFoamClockStep = 0.1;
constexpr int kFoamCoverageDebugView = 2;
constexpr float kWhitecapWind = 8.0f;
constexpr float kBeachWaterlineX = kBasinFarX - (kLandZ - kWaterSurfaceZ) * (kBasinFarX - kBeachStartX) /
                                                    (kLandZ - kBasinFloorZ);
constexpr float kBeachRisePerYard = (kLandZ - kBasinFloorZ) / (kBasinFarX - kBeachStartX);
constexpr float kShoreFoamFadeDepth = 0.8f;
constexpr float kNearShoreDepth = 0.03f;
constexpr float kMidShoreDepth = 0.25f;
constexpr BYTE kNoFoamCoverage = 0;
constexpr BYTE kVisibleFoamCoverage = 16;
constexpr int kNormalDebugView = 1;
constexpr int kEdgeReferenceOffset = 4;
constexpr int kEdgeSearchMargin = 6;
constexpr size_t kMinOccluderEdgePixels = 20;
constexpr double kMinReferenceSlopeDetail = 3.0;
constexpr double kMinEdgeDetailRatio = 0.5;
constexpr Vec3 kOccludedBoxLow = {120, -40, kBasinFloorZ};
constexpr Vec3 kOccludedBoxHigh = {126, -34, 8};
constexpr int kReflectionMatchTolerance = 3;
constexpr size_t kMinWallReflectionPixels = 50;
constexpr double kMinReflectionFogChange = 8.0;
constexpr int kOpaqueLiquidCountWord = 1;
constexpr int kTransparentLiquidCountWord = 5;
constexpr int kTransparentLiquidPass = 1;
constexpr int kMaxMaskRetryPasses = 120;
constexpr int kSkyReflectionsOnlyVariant = 0;
constexpr int kNoForcedVariant = -1;
constexpr int kReflectingQualities[] = {2, 3};
constexpr Vec3 kLowEye = {225, 0, 0.3f};
constexpr Vec3 kLowEyeTarget = {225, 100, 0.3f};
constexpr uint32_t kBlackSky = 0xFF000000;
constexpr int kFogRadianceDebugView = 1;
constexpr float kSkyFogElevationsDegrees[] = {1.0f, 2.0f, 4.0f, 8.0f, 16.0f};
constexpr float kMaxSkyReflectionFogMismatch = 0.05f;
constexpr float kMinComparedFogRadiance = 1e-4f;

const D3DRENDERSTATETYPE kSentinelRenderStates[] = {
    D3DRS_ZENABLE,           D3DRS_ZWRITEENABLE,     D3DRS_ZFUNC,
    D3DRS_ALPHATESTENABLE,   D3DRS_ALPHAREF,         D3DRS_ALPHAFUNC,
    D3DRS_ALPHABLENDENABLE,  D3DRS_SRCBLEND,         D3DRS_DESTBLEND,
    D3DRS_BLENDOP,           D3DRS_SEPARATEALPHABLENDENABLE, D3DRS_CULLMODE,
    D3DRS_SCISSORTESTENABLE, D3DRS_COLORWRITEENABLE, D3DRS_SRGBWRITEENABLE,
    D3DRS_FOGENABLE,         D3DRS_CLIPPLANEENABLE,  D3DRS_FILLMODE,
    D3DRS_STENCILENABLE,     D3DRS_STENCILFUNC,      D3DRS_STENCILPASS,
    D3DRS_STENCILFAIL,       D3DRS_STENCILZFAIL,     D3DRS_STENCILREF,
    D3DRS_STENCILMASK,       D3DRS_STENCILWRITEMASK, D3DRS_TWOSIDEDSTENCILMODE,
    D3DRS_CCW_STENCILFUNC,   D3DRS_CCW_STENCILPASS,  D3DRS_CCW_STENCILFAIL,
    D3DRS_CCW_STENCILZFAIL,
};
constexpr int kSentinelRenderStateCount = sizeof(kSentinelRenderStates) / sizeof(kSentinelRenderStates[0]);

const D3DSAMPLERSTATETYPE kSentinelSamplerStates[] = {
    D3DSAMP_ADDRESSU,  D3DSAMP_ADDRESSV,  D3DSAMP_ADDRESSW,      D3DSAMP_BORDERCOLOR,
    D3DSAMP_MAGFILTER, D3DSAMP_MINFILTER, D3DSAMP_MIPFILTER,     D3DSAMP_MIPMAPLODBIAS,
    D3DSAMP_MAXMIPLEVEL, D3DSAMP_MAXANISOTROPY, D3DSAMP_SRGBTEXTURE,
};
constexpr int kSentinelSamplerStateCount = sizeof(kSentinelSamplerStates) / sizeof(kSentinelSamplerStates[0]);

struct TaggedStencilState
{
    D3DRENDERSTATETYPE state;
    DWORD value;
};

const TaggedStencilState kTaggedStencil[] = {
    {D3DRS_STENCILENABLE, TRUE},
    {D3DRS_STENCILFUNC, D3DCMP_ALWAYS},
    {D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE},
    {D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP},
    {D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP},
    {D3DRS_STENCILMASK, 0xFF},
    {D3DRS_STENCILWRITEMASK, 0xFF},
    {D3DRS_TWOSIDEDSTENCILMODE, FALSE},
};

struct ClientRenderState
{
    D3DRENDERSTATETYPE state;
    DWORD value;
};

const ClientRenderState kClientDrawStates[] = {
    {D3DRS_ZENABLE, D3DZB_TRUE},
    {D3DRS_ZWRITEENABLE, TRUE},
    {D3DRS_ZFUNC, D3DCMP_LESSEQUAL},
    {D3DRS_ALPHATESTENABLE, FALSE},
    {D3DRS_ALPHAREF, kClientAlphaRef},
    {D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL},
    {D3DRS_ALPHABLENDENABLE, FALSE},
    {D3DRS_SRCBLEND, D3DBLEND_SRCALPHA},
    {D3DRS_DESTBLEND, D3DBLEND_DESTCOLOR},
    {D3DRS_BLENDOP, D3DBLENDOP_ADD},
    {D3DRS_SEPARATEALPHABLENDENABLE, FALSE},
    {D3DRS_CULLMODE, D3DCULL_NONE},
    {D3DRS_SCISSORTESTENABLE, FALSE},
    {D3DRS_COLORWRITEENABLE, kClientColourWrite},
    {D3DRS_SRGBWRITEENABLE, FALSE},
    {D3DRS_FOGENABLE, FALSE},
    {D3DRS_CLIPPLANEENABLE, 0},
    {D3DRS_FILLMODE, D3DFILL_SOLID},
    {D3DRS_LIGHTING, FALSE},
};

const ClientRenderState kClientStencilStates[] = {
    {D3DRS_STENCILENABLE, FALSE},
    {D3DRS_STENCILFUNC, D3DCMP_LESS},
    {D3DRS_STENCILPASS, D3DSTENCILOP_INCR},
    {D3DRS_STENCILFAIL, D3DSTENCILOP_DECR},
    {D3DRS_STENCILZFAIL, D3DSTENCILOP_INVERT},
    {D3DRS_STENCILREF, kClientStencilRef},
    {D3DRS_STENCILMASK, kClientStencilMask},
    {D3DRS_STENCILWRITEMASK, kClientStencilWriteMask},
    {D3DRS_TWOSIDEDSTENCILMODE, FALSE},
    {D3DRS_CCW_STENCILFUNC, D3DCMP_GREATER},
    {D3DRS_CCW_STENCILPASS, D3DSTENCILOP_DECRSAT},
    {D3DRS_CCW_STENCILFAIL, D3DSTENCILOP_INCRSAT},
    {D3DRS_CCW_STENCILZFAIL, D3DSTENCILOP_ZERO},
};

struct WaterSentinel
{
    DWORD renderStates[kSentinelRenderStateCount];
    DWORD samplers[kSamplerStages][kSentinelSamplerStateCount];
    IDirect3DBaseTexture9* textures[kSamplerStages];
    IDirect3DVertexShader9* vs;
    IDirect3DPixelShader9* ps;
    IDirect3DVertexDeclaration9* decl;
    IDirect3DVertexBuffer9* stream;
    UINT streamOffset;
    UINT streamStride;
    UINT streamFrequency;
    float constants[kSentinelPixelConstants * 4];
    D3DVIEWPORT9 viewport;
    RECT scissor;
    IDirect3DSurface9* targets[4];
    IDirect3DSurface9* depth;
};

void ReadWaterSentinel(IDirect3DDevice9* dev, WaterSentinel& s)
{
    std::memset(&s, 0, sizeof(s));
    for (int i = 0; i < kSentinelRenderStateCount; ++i)
        dev->GetRenderState(kSentinelRenderStates[i], &s.renderStates[i]);
    for (DWORD stage = 0; stage < kSamplerStages; ++stage)
    {
        for (int i = 0; i < kSentinelSamplerStateCount; ++i)
            dev->GetSamplerState(stage, kSentinelSamplerStates[i], &s.samplers[stage][i]);
        dev->GetTexture(stage, &s.textures[stage]);
    }
    dev->GetVertexShader(&s.vs);
    dev->GetPixelShader(&s.ps);
    dev->GetVertexDeclaration(&s.decl);
    dev->GetStreamSource(0, &s.stream, &s.streamOffset, &s.streamStride);
    dev->GetStreamSourceFreq(0, &s.streamFrequency);
    dev->GetPixelShaderConstantF(0, s.constants, kSentinelPixelConstants);
    dev->GetViewport(&s.viewport);
    dev->GetScissorRect(&s.scissor);
    for (DWORD i = 0; i < 4; ++i)
        dev->GetRenderTarget(i, &s.targets[i]);
    dev->GetDepthStencilSurface(&s.depth);
}

void ReleaseWaterSentinel(WaterSentinel& s)
{
    for (IDirect3DBaseTexture9* texture : s.textures)
        if (texture)
            texture->Release();
    for (IDirect3DSurface9* target : s.targets)
        if (target)
            target->Release();
    IUnknown* refs[] = {s.vs, s.ps, s.decl, s.stream, s.depth};
    for (IUnknown* ref : refs)
        if (ref)
            ref->Release();
}

bool SameWaterSentinel(const WaterSentinel& a, const WaterSentinel& b, bool report)
{
    bool same = true;
    for (int i = 0; i < kSentinelRenderStateCount; ++i)
        if (a.renderStates[i] != b.renderStates[i])
        {
            same = false;
            if (report)
                std::printf("     render state %d: %lu -> %lu\n", kSentinelRenderStates[i], a.renderStates[i],
                            b.renderStates[i]);
        }
    for (DWORD stage = 0; stage < kSamplerStages; ++stage)
    {
        for (int i = 0; i < kSentinelSamplerStateCount; ++i)
            if (a.samplers[stage][i] != b.samplers[stage][i])
            {
                same = false;
                if (report)
                    std::printf("     sampler %lu state %d: %lu -> %lu\n", stage, kSentinelSamplerStates[i],
                                a.samplers[stage][i], b.samplers[stage][i]);
            }
        if (a.textures[stage] != b.textures[stage])
        {
            same = false;
            if (report)
                std::printf("     texture %lu differs\n", stage);
        }
    }
    const bool shaders = a.vs == b.vs && a.ps == b.ps && a.decl == b.decl;
    const bool stream = a.stream == b.stream && a.streamOffset == b.streamOffset &&
                        a.streamStride == b.streamStride && a.streamFrequency == b.streamFrequency;
    const bool constants = std::memcmp(a.constants, b.constants, sizeof(a.constants)) == 0;
    const bool viewport = std::memcmp(&a.viewport, &b.viewport, sizeof(a.viewport)) == 0 &&
                          std::memcmp(&a.scissor, &b.scissor, sizeof(a.scissor)) == 0;
    const bool targets = std::memcmp(a.targets, b.targets, sizeof(a.targets)) == 0 && a.depth == b.depth;
    if (report && !shaders)
        std::printf("     shaders or declaration differ\n");
    if (report && !stream)
        std::printf("     stream 0 differs\n");
    if (report && !constants)
        std::printf("     pixel constants differ\n");
    if (report && !viewport)
        std::printf("     viewport or scissor differs\n");
    if (report && !targets)
        std::printf("     render targets or depth differ\n");
    return same && shaders && stream && constants && viewport && targets;
}

void SetSentinelState(WaterSentinel& s, D3DRENDERSTATETYPE state, DWORD value)
{
    for (int i = 0; i < kSentinelRenderStateCount; ++i)
        if (kSentinelRenderStates[i] == state)
            s.renderStates[i] = value;
}

bool OnlyDocumentedStencilWrites(const WaterSentinel& before, const WaterSentinel& tagged, int waterClass)
{
    WaterSentinel expected = before;
    for (const TaggedStencilState& tag : kTaggedStencil)
        SetSentinelState(expected, tag.state, tag.value);
    SetSentinelState(expected, D3DRS_STENCILREF, static_cast<DWORD>(waterClass));
    return SameWaterSentinel(expected, tagged, true);
}

struct WaterView
{
    Vec3 eye;
    Vec3 at;
    float view[16];
    float proj[16];
    D3DVIEWPORT9 world;
    FrameInputs in;
    WaterInputs water;
    bool sceneStockFogged = false;
};

WaterInputs SyntheticWaterInputs()
{
    WaterInputs water = {};
    const uint32_t sky[kSkyColorCount] = {0xFF3A6FB0, 0xFF5A8CC8, 0xFF7AA6D6, 0xFF9BBEE0, 0xFFB8D0E6, 0xFF9DB2C8};
    std::memcpy(water.skyColors, sky, sizeof(sky));
    water.riverColors[0] = 0xFF2E6E78;
    water.riverColors[1] = 0xFF10323A;
    water.oceanColors[0] = 0xFF1E5A7A;
    water.oceanColors[1] = 0xFF08243A;
    water.stockFogApplies = false;
    return water;
}

WaterView MakeWaterView(Vec3 eyeOffset, Vec3 atOffset)
{
    WaterView v = {};
    v.eye = Add(eyeOffset, kGameLikeWorldOffset);
    v.at = Add(atOffset, kGameLikeWorldOffset);
    v.world = {0, 0, 1280, 688, 0.0f, 1.0f};
    EngineProjection(1280.0f / 688.0f, v.proj);
    CameraRelativeLookAt(v.eye, v.at, v.view);
    v.in = MakeInputs(v.view, v.proj, v.eye, v.at, v.world);
    v.water = SyntheticWaterInputs();
    return v;
}

WaterView DefaultWaterView()
{
    return MakeWaterView({0, 0, 9}, {100, 2, 4});
}

Vec3 PixelRay(const WaterView& v, float px, float py)
{
    const float ndcX = px / v.world.Width * 2.0f - 1.0f;
    const float ndcY = 1.0f - py / v.world.Height * 2.0f;
    const Vec3 ray = {(ndcX - v.proj[8]) / v.proj[0], (ndcY - v.proj[9]) / v.proj[5], 1.0f};
    const float* m = v.view;
    return {ray.x * m[0] + ray.y * m[1] + ray.z * m[2], ray.x * m[4] + ray.y * m[5] + ray.z * m[6],
            ray.x * m[8] + ray.y * m[9] + ray.z * m[10]};
}

float ViewDepthOfPlane(const WaterView& v, Vec3 rayAtUnitViewDepth, float planeZ)
{
    return (planeZ + kGameLikeWorldOffset.z - v.eye.z) / rayAtUnitViewDepth.z;
}

void AddSlope(std::vector<SceneVertex>& v, float x0, float z0, float x1, float z1, float halfWidth, DWORD colour)
{
    AddQuad(v, {x0, -halfWidth, z0}, {x1, -halfWidth, z1}, {x1, halfWidth, z1}, {x0, halfWidth, z0}, colour);
}

std::vector<SceneVertex> BuildBasinScene()
{
    std::vector<SceneVertex> v;
    const float w = kBasinHalfWidth;
    AddQuad(v, {-kLandExtent, -kLandExtent, kLandZ}, {kBasinNearX, -kLandExtent, kLandZ},
            {kBasinNearX, kLandExtent, kLandZ}, {-kLandExtent, kLandExtent, kLandZ}, kLandColour);
    AddQuad(v, {kBasinNearX, w, kLandZ}, {kLandExtent, w, kLandZ}, {kLandExtent, kLandExtent, kLandZ},
            {kBasinNearX, kLandExtent, kLandZ}, kLandColour);
    AddQuad(v, {kBasinNearX, -kLandExtent, kLandZ}, {kLandExtent, -kLandExtent, kLandZ}, {kLandExtent, -w, kLandZ},
            {kBasinNearX, -w, kLandZ}, kLandColour);
    AddQuad(v, {kBasinFarX, -w, kLandZ}, {kLandExtent, -w, kLandZ}, {kLandExtent, w, kLandZ}, {kBasinFarX, w, kLandZ},
            kLandColour);
    AddSlope(v, kBasinNearX, kBasinFloorZ, kBeachStartX, kBasinFloorZ, w, kFloorColour);
    AddSlope(v, kBeachStartX, kBasinFloorZ, kBasinFarX, kLandZ, w, kFloorColour);
    AddQuad(v, {kBasinNearX, -w, kBasinFloorZ}, {kBasinNearX, w, kBasinFloorZ}, {kBasinNearX, w, kLandZ},
            {kBasinNearX, -w, kLandZ}, kWallColour);
    AddQuad(v, {kBasinNearX, w, kBasinFloorZ}, {kBasinFarX, w, kBasinFloorZ}, {kBasinFarX, w, kLandZ},
            {kBasinNearX, w, kLandZ}, kWallColour);
    AddQuad(v, {kBasinNearX, -w, kBasinFloorZ}, {kBasinFarX, -w, kBasinFloorZ}, {kBasinFarX, -w, kLandZ},
            {kBasinNearX, -w, kLandZ}, kWallColour);
    AddBox(v, {120, -40, kBasinFloorZ}, {126, -34, 8}, kBoxColour);
    AddBox(v, {200, 30, kBasinFloorZ}, {206, 36, 12}, kBoxColour);
    AddBox(v, {260, -90, kBasinFloorZ}, {270, -80, 6}, kBoxColour);
    AddBox(v, {600, -300, kLandZ}, {640, 300, 90}, kFarWallColour);
    return v;
}

struct WaterStrip
{
    float y0;
    float y1;
    int waterClass;
    DWORD colour;
    float z = kWaterSurfaceZ;
    bool hooked = true;
};

const WaterStrip kLakeBasin[] = {{-kBasinHalfWidth, kBasinHalfWidth, kLakeClass, kStockWaterColour}};
const WaterStrip kClassStrips[] = {
    {90.0f, kBasinHalfWidth, kLakeClass, kStockWaterColour},
    {-80.0f, 90.0f, kOceanClass, kStockWaterColour},
    {-kBasinHalfWidth, -80.0f, kRiverClass, kStockWaterColour},
};
const WaterStrip kHiddenLake[] = {{-kBasinHalfWidth, kBasinHalfWidth, kLakeClass, kInvisibleWaterColour}};
const WaterStrip kInteriorBasin[] = {{-kBasinHalfWidth, kBasinHalfWidth, kInteriorClass, kStockWaterColour}};
const WaterStrip kCoverLiquids[] = {
    {-60.0f, -20.0f, static_cast<int>(WaterClass::None), kCoverLiquidColour, kCoverLiquidZ, true},
    {20.0f, 60.0f, static_cast<int>(WaterClass::None), kCoverLiquidColour, kCoverLiquidZ, false},
};

enum class WaterCalls
{
    None,
    Pass,
    PassWithoutTags,
    Hooks,
};

const uint32_t* QueuedLiquidRenderer()
{
    static uint32_t liquidRenderer[kLiquidRendererWords] = {};
    liquidRenderer[kTransparentLiquidCountWord] = 1;
    return liquidRenderer;
}

struct WaterFrame
{
    const WaterStrip* strips = kLakeBasin;
    int stripCount = 1;
    WaterCalls calls = WaterCalls::Pass;
    const WaterStrip* hiddenTaggedStrips = nullptr;
    int hiddenTaggedStripCount = 0;
    const WaterStrip* covers = nullptr;
    int coverCount = 0;
    bool opaqueMask = false;
    bool fogDepthView = false;
    int fault = kNoWaterFault;
    IDirect3DSurface9* depthAtEnd = nullptr;
};

struct WaterFrameResult
{
    Image image;
    bool began = false;
    const char* skip = "";
    bool stateKept = true;
    bool armWritesDocumented = true;
    bool tagWritesDocumented = true;
    bool untagRestores = true;
    bool faulted = false;
    bool fogRendered = false;
    bool shaded = false;
    const char* endSkip = "";
};

bool BeginWaterFaulted(const FrameInputs* in, const WaterInputs* water, const char** skip, bool* began)
{
    __try
    {
        *began = vf_test_water_begin(in, water, skip) != 0;
        return false;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        vf_test_water_abort();
        return true;
    }
}

bool EndWaterFaulted(const char** skip, bool* shaded)
{
    __try
    {
        *shaded = vf_test_water_end(skip) != 0;
        return false;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        vf_test_water_abort();
        return true;
    }
}

ULONG References(IUnknown* object)
{
    object->AddRef();
    return object->Release();
}

class BasinClient
{
public:
    BasinClient(Harness& h, const WaterView& v) : m_h(h), m_v(v), m_scene(BuildBasinScene()) {}

    WaterFrameResult Render(const WaterFrame& frame)
    {
        IDirect3DDevice9* dev = m_h.dev;
        WaterFrameResult result;
        BeginClientFrame();
        DrawScene();
        ApplyClientState();
        WaterSentinel before;
        ReadWaterSentinel(dev, before);
        const bool hooked = frame.calls == WaterCalls::Hooks;
        vf_test_fail_water_in_window(frame.fault);
        if (hooked)
        {
            vf_test_use_water_hook_client(&m_v.in, &m_v.water);
            vf_test_hook_water_pass_begin(QueuedLiquidRenderer());
            result.began = vf_test_water_armed() != 0;
        }
        else if (frame.calls != WaterCalls::None)
            result.faulted = BeginWaterFaulted(&m_v.in, &m_v.water, &result.skip, &result.began);
        WaterSentinel armed;
        ReadWaterSentinel(dev, armed);
        result.armWritesDocumented = result.began ? OnlyDocumentedStencilWrites(before, armed, 0)
                                                  : SameWaterSentinel(before, armed, true);
        ReleaseWaterSentinel(armed);
        const bool tagged = frame.calls == WaterCalls::Pass || hooked;
        for (int i = 0; i < frame.stripCount; ++i)
            DrawStrip(frame.strips[i], tagged, hooked, frame.opaqueMask, result);
        for (int i = 0; i < frame.hiddenTaggedStripCount; ++i)
            DrawStrip(frame.hiddenTaggedStrips[i], true, hooked, false, result);
        for (int i = 0; i < frame.coverCount; ++i)
            DrawStrip(frame.covers[i], tagged, hooked, frame.opaqueMask, result);
        m_h.DrawPretransformedQuadAtRawDepth(kOtherPassLeft, kOtherPassTop, kOtherPassRight, kOtherPassBottom,
                                             kOtherPassRawDepth, kOtherPassColour);
        ApplyClientDrawState();
        IDirect3DSurface9* clientDepth = nullptr;
        if (frame.depthAtEnd)
        {
            dev->GetDepthStencilSurface(&clientDepth);
            dev->SetDepthStencilSurface(frame.depthAtEnd);
        }
        if (hooked)
            vf_test_hook_water_pass_end();
        else if (frame.calls != WaterCalls::None)
            result.faulted = EndWaterFaulted(&result.endSkip, &result.shaded) || result.faulted;
        if (frame.depthAtEnd)
        {
            dev->SetDepthStencilSurface(clientDepth);
            clientDepth->Release();
        }
        vf_test_fail_water_in_window(kNoWaterFault);
        WaterSentinel after;
        ReadWaterSentinel(dev, after);
        result.stateKept = SameWaterSentinel(before, after, true);
        ReleaseWaterSentinel(before);
        ReleaseWaterSentinel(after);
        if (frame.fogDepthView)
        {
            const char* skip = "";
            result.fogRendered = vf_test_render(&m_v.in, &skip) != 0;
        }
        result.image = Capture(dev);
        dev->EndScene();
        dev->Present(nullptr, nullptr, nullptr, nullptr);
        if (hooked)
            vf_test_hook_frame_end();
        return result;
    }

    const WaterView& View() const { return m_v; }

private:
    void BeginClientFrame()
    {
        IDirect3DDevice9* dev = m_h.dev;
        dev->BeginScene();
        const D3DVIEWPORT9 full = {0, 0, m_h.pp.BackBufferWidth, m_h.pp.BackBufferHeight, 0.0f, 1.0f};
        dev->SetViewport(&full);
        dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
        dev->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, m_h.clearColor, 1.0f, 0);
    }

    void DrawScene()
    {
        IDirect3DDevice9* dev = m_h.dev;
        float d3dProj[16];
        RemapToD3DDepthRange(m_v.proj, d3dProj);
        D3DMATRIX cameraRelativeWorld = {};
        cameraRelativeWorld._11 = cameraRelativeWorld._22 = cameraRelativeWorld._33 = cameraRelativeWorld._44 = 1.0f;
        cameraRelativeWorld._41 = -m_v.eye.x;
        cameraRelativeWorld._42 = -m_v.eye.y;
        cameraRelativeWorld._43 = -m_v.eye.z;
        dev->SetTransform(D3DTS_WORLD, &cameraRelativeWorld);
        dev->SetTransform(D3DTS_VIEW, reinterpret_cast<const D3DMATRIX*>(m_v.view));
        dev->SetTransform(D3DTS_PROJECTION, reinterpret_cast<const D3DMATRIX*>(d3dProj));
        dev->SetViewport(&m_v.world);
        dev->SetVertexShader(nullptr);
        dev->SetPixelShader(nullptr);
        dev->SetRenderState(D3DRS_LIGHTING, FALSE);
        dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        dev->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        dev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        ApplySceneStockFog();
        dev->SetTexture(0, nullptr);
        SelectDiffuseColour();
        dev->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
        dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, static_cast<UINT>(m_scene.size() / 3), m_scene.data(),
                             sizeof(SceneVertex));
    }

    void ApplySceneStockFog()
    {
        IDirect3DDevice9* dev = m_h.dev;
        dev->SetRenderState(D3DRS_FOGENABLE, m_v.sceneStockFogged ? TRUE : FALSE);
        if (!m_v.sceneStockFogged)
            return;
        const float start = m_v.in.fogStart;
        const float end = m_v.in.fogEnd;
        DWORD startBits = 0;
        DWORD endBits = 0;
        std::memcpy(&startBits, &start, sizeof(startBits));
        std::memcpy(&endBits, &end, sizeof(endBits));
        dev->SetRenderState(D3DRS_FOGCOLOR, m_v.in.fogColor);
        dev->SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_NONE);
        dev->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_LINEAR);
        dev->SetRenderState(D3DRS_FOGSTART, startBits);
        dev->SetRenderState(D3DRS_FOGEND, endBits);
    }

    void SelectDiffuseColour()
    {
        IDirect3DDevice9* dev = m_h.dev;
        dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
        dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
        dev->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    }

    void ApplyClientDrawState()
    {
        IDirect3DDevice9* dev = m_h.dev;
        for (const ClientRenderState& state : kClientDrawStates)
            dev->SetRenderState(state.state, state.value);
        dev->SetVertexShader(nullptr);
        dev->SetPixelShader(nullptr);
        dev->SetVertexDeclaration(m_h.engineDecl);
        dev->SetStreamSource(0, m_h.dummyBuffer, kClientStreamOffset, kClientStreamStride);
        dev->SetStreamSourceFreq(0, 1);
        dev->SetViewport(&m_v.world);
        dev->SetScissorRect(&kClientScissor);
    }

    void ApplyClientState()
    {
        IDirect3DDevice9* dev = m_h.dev;
        ApplyClientDrawState();
        for (const ClientRenderState& state : kClientStencilStates)
            dev->SetRenderState(state.state, state.value);
        for (DWORD stage = 0; stage < kSamplerStages; ++stage)
        {
            dev->SetTexture(stage, m_h.dummyTexture);
            dev->SetSamplerState(stage, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR);
            dev->SetSamplerState(stage, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
            dev->SetSamplerState(stage, D3DSAMP_ADDRESSW, D3DTADDRESS_MIRRORONCE);
            dev->SetSamplerState(stage, D3DSAMP_BORDERCOLOR, kClientBorderColour + stage);
            dev->SetSamplerState(stage, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC);
            dev->SetSamplerState(stage, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
            dev->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_POINT);
            dev->SetSamplerState(stage, D3DSAMP_MAXMIPLEVEL, stage % 3);
            dev->SetSamplerState(stage, D3DSAMP_MAXANISOTROPY, kClientMaxAnisotropy);
        }
        float constants[kSentinelPixelConstants * 4];
        for (UINT i = 0; i < kSentinelPixelConstants * 4; ++i)
            constants[i] = 0.375f * i - 7.0f;
        dev->SetPixelShaderConstantF(0, constants, kSentinelPixelConstants);
    }

    bool Tag(const WaterStrip& strip, bool throughHooks)
    {
        if (throughHooks)
            return vf_test_hook_water_draw_tag(&strip.waterClass) != 0;
        vf_test_water_tag(strip.waterClass);
        return true;
    }

    void DrawStrip(const WaterStrip& strip, bool tag, bool throughHooks, bool opaqueMask, WaterFrameResult& result)
    {
        IDirect3DDevice9* dev = m_h.dev;
        WaterSentinel beforeTag;
        ReadWaterSentinel(dev, beforeTag);
        bool tagged = false;
        if (tag && strip.hooked)
        {
            tagged = Tag(strip, throughHooks);
            WaterSentinel afterTag;
            ReadWaterSentinel(dev, afterTag);
            const bool documented = result.began ? OnlyDocumentedStencilWrites(beforeTag, afterTag, strip.waterClass)
                                                 : SameWaterSentinel(beforeTag, afterTag, true);
            result.tagWritesDocumented = result.tagWritesDocumented && documented;
            ReleaseWaterSentinel(afterTag);
        }
        const DWORD colour = opaqueMask ? kWaterMaskColour : strip.colour;
        SceneVertex quad[6];
        const Vec3 corners[4] = {{kBasinNearX, strip.y0, strip.z}, {kBasinFarX, strip.y0, strip.z},
                                 {kBasinFarX, strip.y1, strip.z}, {kBasinNearX, strip.y1, strip.z}};
        const int order[6] = {0, 1, 2, 0, 2, 3};
        for (int i = 0; i < 6; ++i)
        {
            const Vec3 p = Add(corners[order[i]], kGameLikeWorldOffset);
            quad[i] = {p.x, p.y, p.z, colour};
        }
        SelectDiffuseColour();
        dev->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
        dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, opaqueMask ? FALSE : TRUE);
        dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        dev->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
        dev->SetRenderState(D3DRS_ALPHAREF, kStockWaterAlphaRef);
        dev->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
        dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, quad, sizeof(SceneVertex));
        ApplyClientDrawState();
        if (tagged)
        {
            if (throughHooks)
                vf_test_hook_water_draw_untag();
            else
                vf_test_water_untag();
            WaterSentinel untagged;
            ReadWaterSentinel(dev, untagged);
            result.untagRestores = result.untagRestores && SameWaterSentinel(beforeTag, untagged, true);
            ReleaseWaterSentinel(untagged);
        }
        ReleaseWaterSentinel(beforeTag);
    }

    Harness& m_h;
    WaterView m_v;
    std::vector<SceneVertex> m_scene;
};

void Set4(float (&v)[4], float x, float y, float z, float w)
{
    v[0] = x;
    v[1] = y;
    v[2] = z;
    v[3] = w;
}

struct SyntheticWaterData
{
    std::vector<WaterPreset> presets;
    std::vector<WaterFftTile> tiles;
    std::vector<std::vector<std::vector<uint8_t>>> maskLevels;
    std::vector<WaterMask> maskInfo;
};

WaterPreset SyntheticLake()
{
    WaterPreset p = {};
    p.foreverLiquidId = kForeverGenericLake;
    Set4(p.absorption, 0.04f, 0.015f, 0.008f, 1.0f);
    Set4(p.scatteringIntensities, 0.6f, 0.4f, 0.3f, 0.2f);
    Set4(p.scatteringTop, 0.10f, 0.32f, 0.36f, 1.0f);
    Set4(p.scatteringBottom, 0.02f, 0.10f, 0.12f, 0.45f);
    Set4(p.depthFadeFoam, 0.6f, 24.0f, 1.2f, 0.01f);
    Set4(p.shoreFoam, 0.5f, 18.0f, 0.8f, 0.012f);
    Set4(p.waveFoam, 1.0f, 0.01f, 0.0f, 0.0f);
    Set4(p.waveFoamScaling, 20.0f, 18.0f, 16.0f, 0.0f);
    Set4(p.roughness, 0.05f, 0.08f, 1.0f, 0.0f);
    const int32_t tiles[kWaterPresetTiles] = {0, 1, 2, kWaterNoIndex};
    const int32_t masks[kWaterMaskSlots] = {0, 0, 1, 1, 0, kWaterNoIndex};
    std::memcpy(p.tiles, tiles, sizeof(tiles));
    std::memcpy(p.masks, masks, sizeof(masks));
    return p;
}

WaterPreset SyntheticOcean()
{
    WaterPreset p = SyntheticLake();
    p.foreverLiquidId = kForeverGenericOcean;
    Set4(p.absorption, 0.08f, 0.03f, 0.012f, 1.0f);
    Set4(p.scatteringTop, 0.05f, 0.22f, 0.32f, 1.2f);
    Set4(p.scatteringBottom, 0.01f, 0.05f, 0.10f, 0.5f);
    const int32_t tiles[kWaterPresetTiles] = {0, 1, kWaterNoIndex, kWaterNoIndex};
    const int32_t masks[kWaterMaskSlots] = {1, 1, 1, 0, 1, kWaterNoIndex};
    std::memcpy(p.tiles, tiles, sizeof(tiles));
    std::memcpy(p.masks, masks, sizeof(masks));
    return p;
}

WaterPreset SyntheticInterior()
{
    WaterPreset p = SyntheticLake();
    p.foreverLiquidId = kForeverWmoInterior;
    const int32_t tiles[kWaterPresetTiles] = {2, kWaterNoIndex, kWaterNoIndex, kWaterNoIndex};
    std::memcpy(p.tiles, tiles, sizeof(tiles));
    for (int32_t& mask : p.masks)
        mask = kWaterNoIndex;
    return p;
}

WaterFftTile SyntheticTile(uint32_t id, float size, float amplitude)
{
    WaterFftTile tile = {};
    tile.foreverTileId = id;
    tile.size = size;
    tile.amplitude = amplitude;
    tile.windMultiplier = 1.0f;
    tile.windAlignment = 1.0f;
    const float foam[3] = {0.1f, 0.6f, 0.15f};
    const float oxygen[3] = {0.2f, 0.4f, 0.1f};
    std::memcpy(tile.foam, foam, sizeof(foam));
    std::memcpy(tile.oxygen, oxygen, sizeof(oxygen));
    return tile;
}

std::vector<std::vector<uint8_t>> MaskMipChain(bool checkered)
{
    std::vector<std::vector<uint8_t>> levels;
    std::vector<uint8_t> top(static_cast<size_t>(kMaskSide) * kMaskSide);
    for (int y = 0; y < kMaskSide; ++y)
        for (int x = 0; x < kMaskSide; ++x)
            top[static_cast<size_t>(y) * kMaskSide + x] = checkered && (x + y) % 2 ? 0 : 255;
    levels.push_back(top);
    for (int side = kMaskSide / 2; side >= 1; side /= 2)
    {
        const std::vector<uint8_t>& above = levels.back();
        const int aboveSide = side * 2;
        std::vector<uint8_t> level(static_cast<size_t>(side) * side);
        for (int y = 0; y < side; ++y)
            for (int x = 0; x < side; ++x)
            {
                int sum = 0;
                for (int k = 0; k < 4; ++k)
                    sum += above[static_cast<size_t>(y * 2 + k / 2) * aboveSide + x * 2 + k % 2];
                level[static_cast<size_t>(y) * side + x] = static_cast<uint8_t>((sum + 2) / 4);
            }
        levels.push_back(level);
    }
    return levels;
}

SyntheticWaterData MakeSyntheticWaterData()
{
    SyntheticWaterData data;
    data.presets = {SyntheticLake(), SyntheticOcean(), SyntheticInterior()};
    data.tiles = {SyntheticTile(1001, 32.0f, 1.0f), SyntheticTile(1002, 64.0f, 0.6f), SyntheticTile(1003, 16.0f, 0.8f)};
    for (int checkered = 0; checkered < 2; ++checkered)
    {
        data.maskLevels.push_back(MaskMipChain(checkered != 0));
        WaterMask info = {};
        info.foreverFileDataId = 7475833u + checkered;
        info.size = kMaskSide;
        info.mipCount = static_cast<uint32_t>(data.maskLevels.back().size());
        const float low[3] = {0.70f, 0.75f, 0.80f};
        const float high[3] = {0.97f, 0.98f, 1.00f};
        std::memcpy(info.tintLow, low, sizeof(low));
        std::memcpy(info.tintHigh, high, sizeof(high));
        data.maskInfo.push_back(info);
    }
    return data;
}

bool AssignWaterData(const SyntheticWaterData& data)
{
    std::vector<std::vector<const uint8_t*>> levelPointers;
    std::vector<WaterMaskView> views;
    for (size_t m = 0; m < data.maskInfo.size(); ++m)
    {
        std::vector<const uint8_t*> pointers;
        for (const std::vector<uint8_t>& level : data.maskLevels[m])
            pointers.push_back(level.data());
        levelPointers.push_back(pointers);
    }
    for (size_t m = 0; m < data.maskInfo.size(); ++m)
        views.push_back({data.maskInfo[m], levelPointers[m].data()});
    return vf_test_assign_water_data(data.presets.data(), static_cast<int>(data.presets.size()), data.tiles.data(),
                                     static_cast<int>(data.tiles.size()), views.data(),
                                     static_cast<int>(views.size())) != 0;
}

Config WaterConfig(const Config& base)
{
    Config cfg = base;
    cfg.water = true;
    cfg.waterDebugView = 0;
    cfg.debugView = 0;
    cfg.temporal = 0.0f;
    cfg.godRays = 0.0f;
    cfg.noiseAmount = 0.0f;
    return cfg;
}

Config OpticsOnlyConfig(const Config& base)
{
    Config cfg = WaterConfig(base);
    cfg.waterWaves = 0.0f;
    cfg.waterFoam = 0.0f;
    cfg.waterReflections = 0.0f;
    cfg.waterSpecular = 0.0f;
    cfg.waterZoneColors = 0.0f;
    cfg.waterClarity = 1.0f;
    return cfg;
}

bool SameImage(const Image& a, const Image& b)
{
    return a.w == b.w && a.h == b.h && a.bgra == b.bgra;
}

bool IsMaskPixel(const Image& mask, UINT x, UINT y)
{
    const unsigned char* p = mask.At(x, y);
    return p[0] == 255 && p[1] == 0 && p[2] == 255;
}

struct MaskComparison
{
    size_t waterPixels = 0;
    size_t changedWater = 0;
    size_t changedElsewhere = 0;
};

MaskComparison CompareByMask(const Image& mask, const Image& a, const Image& b)
{
    MaskComparison c;
    for (UINT y = 0; y < a.h; ++y)
        for (UINT x = 0; x < a.w; ++x)
        {
            const bool changed = std::memcmp(a.At(x, y), b.At(x, y), 3) != 0;
            if (IsMaskPixel(mask, x, y))
            {
                ++c.waterPixels;
                c.changedWater += changed ? 1 : 0;
            }
            else
                c.changedElsewhere += changed ? 1 : 0;
        }
    return c;
}

void SaveImage(const std::wstring& outDir, const wchar_t* name, const Image& image)
{
    SavePng(outDir + L"\\" + name + L".png", image.w, image.h, image.bgra);
}

float Decode(unsigned char encoded)
{
    return std::pow(encoded / 255.0f, kDisplayGamma);
}

float Encode(float linear)
{
    return std::pow(std::fmin(std::fmax(linear, 0.0f), 1.0f), 1.0f / kDisplayGamma);
}

struct AbsorptionSample
{
    UINT x;
    UINT y;
    float waterZ;
    float floorZ;
};

std::vector<AbsorptionSample> FlatFloorSamples(const WaterView& v)
{
    std::vector<AbsorptionSample> samples;
    const UINT column = v.world.Width / 2;
    for (UINT row = v.world.Height / 2; row < v.world.Height && samples.size() < 6; row += 9)
    {
        const Vec3 ray = PixelRay(v, column + 0.5f, row + 0.5f);
        if (ray.z >= 0.0f)
            continue;
        const float waterZ = ViewDepthOfPlane(v, ray, kWaterSurfaceZ);
        const float floorZ = ViewDepthOfPlane(v, ray, kBasinFloorZ);
        const float waterX = v.eye.x - kGameLikeWorldOffset.x + ray.x * waterZ;
        const float floorX = v.eye.x - kGameLikeWorldOffset.x + ray.x * floorZ;
        if (waterX > kBasinNearX + 5.0f && floorX < kBeachStartX - 10.0f)
            samples.push_back({column, row, waterZ, floorZ});
    }
    return samples;
}

float WorstAbsorptionError(const Image& image, const std::vector<AbsorptionSample>& samples,
                           const WaterPreset& preset, bool print)
{
    const unsigned char floorBgr[3] = {static_cast<unsigned char>(kFloorColour & 0xFF),
                                       static_cast<unsigned char>((kFloorColour >> 8) & 0xFF),
                                       static_cast<unsigned char>((kFloorColour >> 16) & 0xFF)};
    float worst = samples.empty() ? 1.0f : 0.0f;
    for (const AbsorptionSample& s : samples)
    {
        const float path = s.floorZ - s.waterZ;
        const unsigned char* got = image.At(s.x, s.y);
        for (int channel = 0; channel < 3; ++channel)
        {
            const int rgb = 2 - channel;
            const float transmittance = std::exp(-preset.absorption[rgb] * preset.absorption[3] * path);
            const float expected = Encode(Decode(floorBgr[channel]) * transmittance);
            worst = std::fmax(worst, std::fabs(got[channel] / 255.0f - expected));
        }
        if (print)
            std::printf("     water at %.1f yd over floor at %.1f yd (path %.1f yd): pixel %u,%u = %u %u %u\n",
                        s.waterZ, s.floorZ, path, s.x, s.y, got[2], got[1], got[0]);
    }
    return worst;
}

UINT DepthViewYardsAt(const Image& depthView, UINT x, UINT y)
{
    return depthView.At(x, y)[2];
}

void CheckSkipsAndUntouchedFrames(Harness& h, BasinClient& client, const Config& base, const Image& stock)
{
    const WaterView& v = client.View();
    Config off = WaterConfig(base);
    off.water = false;
    vf_test_set_config(&off);
    WaterFrame calls;
    const WaterFrameResult disabled = client.Render(calls);
    Check(!disabled.began && std::strcmp(disabled.skip, "water disabled") == 0 && SameImage(disabled.image, stock),
          "Water=0 skips the water pass and leaves the frame bit-identical to one without the calls");

    Config on = WaterConfig(base);
    vf_test_set_config(&on);
    WaterView underwater = v;
    underwater.in.inLiquid = true;
    BasinClient diver(h, underwater);
    const WaterFrameResult submerged = diver.Render(calls);
    Check(!submerged.began && std::strcmp(submerged.skip, "camera under water") == 0 &&
              SameImage(submerged.image, stock),
          "a camera in liquid skips the water pass and keeps the client's water");

    const int noData = vf_test_assign_water_data(nullptr, -1, nullptr, 0, nullptr, 0);
    const WaterFrameResult missing = client.Render(calls);
    Check(noData == 0 && !missing.began && std::strcmp(missing.skip, "no water data") == 0 &&
              SameImage(missing.image, stock),
          "without water data the pass is skipped and the frame is untouched");
}

void CheckQueuedTransparentLiquidsDecideArming()
{
    uint32_t liquidRenderer[kLiquidRendererWords] = {};
    liquidRenderer[kOpaqueLiquidCountWord] = 5;
    const bool emptySkips = vf_test_transparent_liquids_queued(liquidRenderer) == 0;
    liquidRenderer[kTransparentLiquidCountWord] = 3;
    const bool queuedArms = vf_test_transparent_liquids_queued(liquidRenderer) == 1;
    const bool noRendererSkips = vf_test_transparent_liquids_queued(nullptr) == 0;
    Check(emptySkips && queuedArms && noRendererSkips,
          "the water pass arms only when the client's transparent liquid bucket ([renderer+0x14]) holds draws");
}

struct WaterPassCall
{
    const void* liquidRenderer;
    const void* camera;
    int pass;
};

WaterPassCall g_waterPassCall = {};

void __fastcall RecordWaterPassCall(const void* liquidRenderer, void*, const void* camera, int pass)
{
    g_waterPassCall = {liquidRenderer, camera, pass};
}

void CallThroughWaterPassThunk(const void* thunk, const void* liquidRenderer, const void* camera, int pass)
{
    __asm {
        mov ecx, liquidRenderer
        push pass
        push camera
        call thunk
    }
}

void CheckWaterPassThunkKeepsTheRenderer()
{
    const uint32_t emptyRenderer[kLiquidRendererWords] = {};
    const float camera[3] = {};
    const void* thunk = vf_test_water_pass_thunk(reinterpret_cast<uintptr_t>(&RecordWaterPassCall));
    vf_test_water_pass_begin_reuses_argument_slot(1);
    g_waterPassCall = {};
    CallThroughWaterPassThunk(thunk, emptyRenderer, camera, kTransparentLiquidPass);
    vf_test_water_pass_begin_reuses_argument_slot(0);
    std::printf("     client pass called with renderer %p (given %p), camera %p (given %p), pass %d\n",
                g_waterPassCall.liquidRenderer, static_cast<const void*>(emptyRenderer), g_waterPassCall.camera,
                static_cast<const void*>(camera), g_waterPassCall.pass);
    Check(g_waterPassCall.liquidRenderer == emptyRenderer && g_waterPassCall.camera == camera &&
              g_waterPassCall.pass == kTransparentLiquidPass,
          "the water pass thunk calls the client's pass with its liquid renderer (ecx) and arguments even when the "
          "begin hook reuses its own argument slot");
}

void CheckIdleTagLeavesState(Harness& h)
{
    WaterSentinel before;
    ReadWaterSentinel(h.dev, before);
    vf_test_water_tag(kLakeClass);
    vf_test_water_untag();
    WaterSentinel after;
    ReadWaterSentinel(h.dev, after);
    Check(SameWaterSentinel(before, after, true), "tagging outside an armed water pass leaves the device state alone");
    ReleaseWaterSentinel(before);
    ReleaseWaterSentinel(after);
}

void CheckDepthWriteNesting(Harness& h, BasinClient& client)
{
    IDirect3DDevice9* dev = h.dev;
    const WaterView& v = client.View();
    WaterInputs water = v.water;
    dev->BeginScene();
    dev->SetViewport(&v.world);
    DWORD forcedAtBegin = FALSE;
    DWORD duringWater = FALSE;
    DWORD afterWater = FALSE;
    DWORD restored = TRUE;
    dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    vf_test_force_depth_write(1);
    const char* skip = "";
    const bool began = vf_test_water_begin(&v.in, &water, &skip) != 0;
    dev->GetRenderState(D3DRS_ZWRITEENABLE, &forcedAtBegin);
    dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    dev->GetRenderState(D3DRS_ZWRITEENABLE, &duringWater);
    vf_test_water_end(nullptr);
    dev->GetRenderState(D3DRS_ZWRITEENABLE, &afterWater);
    vf_test_force_depth_write(0);
    dev->GetRenderState(D3DRS_ZWRITEENABLE, &restored);
    Check(began && forcedAtBegin == TRUE && duringWater == TRUE && afterWater == TRUE && restored == FALSE,
          "water depth forcing nests inside the liquid depth forcing and restores the client's last request");

    DWORD suppressed = TRUE;
    DWORD unsuppressed = FALSE;
    DWORD ended = FALSE;
    dev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    const bool again = vf_test_water_begin(&v.in, &water, &skip) != 0;
    dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    vf_test_suppress_depth_write(1);
    dev->GetRenderState(D3DRS_ZWRITEENABLE, &suppressed);
    vf_test_suppress_depth_write(0);
    dev->GetRenderState(D3DRS_ZWRITEENABLE, &unsuppressed);
    vf_test_water_end(nullptr);
    dev->GetRenderState(D3DRS_ZWRITEENABLE, &ended);
    Check(again && suppressed == FALSE && unsuppressed == TRUE && ended == FALSE,
          "text depth suppression overrides the water depth forcing and the client's request returns after it");
    dev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    dev->EndScene();
}

void CheckOpticsAgainstReference(BasinClient& client, const Config& base, const std::wstring& outDir)
{
    SyntheticWaterData data = MakeSyntheticWaterData();
    for (WaterPreset& preset : data.presets)
        Set4(preset.scatteringIntensities, 0.0f, 0.0f, 0.0f, 0.0f);
    const bool assigned = AssignWaterData(data);
    const Config optics = OpticsOnlyConfig(base);
    vf_test_set_config(&optics);
    const std::vector<AbsorptionSample> samples = FlatFloorSamples(client.View());
    WaterFrame frame;
    const WaterFrameResult result = client.Render(frame);
    SaveImage(outDir, L"water-absorption", result.image);
    const float error = WorstAbsorptionError(result.image, samples, data.presets[0], true);
    std::printf("     flat water absorption: %zu samples, worst error %.2f/255\n", samples.size(), error * 255.0f);
    Check(assigned && result.began && samples.size() >= 4 && error <= kReferenceTolerance,
          "flat water refraction and Beer-Lambert absorption match the CPU reference");

    vf_test_force_packed_water_depth(1);
    const WaterFrameResult packed = client.Render(frame);
    vf_test_force_packed_water_depth(0);
    const float packedError = WorstAbsorptionError(packed.image, samples, data.presets[0], false);
    std::printf("     packed depth copies: worst absorption error %.2f/255\n", packedError * 255.0f);
    Check(packed.began && packedError <= kReferenceTolerance,
          "the packed rgba8 depth copies reproduce the float-copy absorption");

    Config clearer = optics;
    clearer.waterClarity = 2.0f;
    vf_test_set_config(&clearer);
    const WaterFrameResult clear = client.Render(frame);
    WaterPreset halved = data.presets[0];
    halved.absorption[3] *= 0.5f;
    const float clarityError = WorstAbsorptionError(clear.image, samples, halved, false);
    std::printf("     WaterClarity 2: worst error %.2f/255 against half the absorption\n", clarityError * 255.0f);
    Check(clear.began && clarityError <= kReferenceTolerance, "WaterClarity divides the absorption coefficient");
    AssignWaterData(MakeSyntheticWaterData());
}

void CheckWaterDepthReachesFog(Harness& h, const Config& base)
{
    WaterView view = DefaultWaterView();
    view.in.farClip = kDepthViewRange;
    BasinClient client(h, view);
    const std::vector<AbsorptionSample> samples = FlatFloorSamples(view);
    if (samples.empty())
    {
        Check(false, "the basin view has water over the flat floor");
        return;
    }
    const AbsorptionSample& s = samples.back();
    Config depthView = WaterConfig(base);
    depthView.debugView = 3;
    depthView.maxDistance = kDepthViewRange;
    WaterFrame frame;
    frame.fogDepthView = true;
    vf_test_set_config(&depthView);
    const WaterFrameResult water = client.Render(frame);
    depthView.water = false;
    vf_test_set_config(&depthView);
    const WaterFrameResult stock = client.Render(frame);
    const UINT withWater = DepthViewYardsAt(water.image, s.x, s.y);
    const UINT withoutWater = DepthViewYardsAt(stock.image, s.x, s.y);
    std::printf("     fog linear depth at %u,%u: %u yd with the water pass, %u yd without (surface %.1f, floor %.1f)\n",
                s.x, s.y, withWater, withoutWater, s.waterZ, s.floorZ);
    Check(water.began && std::fabs(withWater - s.waterZ) < 2.0f && std::fabs(withoutWater - s.floorZ) < 2.0f,
          "the water pass forces the water surface depth into INTZ for the fog");
}

void CheckStencilClearedEachPass(BasinClient& client, const Config& base)
{
    const Config on = WaterConfig(base);
    Config off = on;
    off.water = false;
    WaterFrame tagged;
    WaterFrame untagged;
    untagged.calls = WaterCalls::PassWithoutTags;
    untagged.hiddenTaggedStrips = kHiddenLake;
    untagged.hiddenTaggedStripCount = 1;
    vf_test_set_config(&on);
    const WaterFrameResult first = client.Render(tagged);
    const WaterFrameResult second = client.Render(untagged);
    vf_test_set_config(&off);
    const WaterFrameResult firstStock = client.Render(tagged);
    const WaterFrameResult secondStock = client.Render(untagged);
    Check(first.began && !SameImage(first.image, firstStock.image), "a tagged water frame is shaded");
    Check(second.began && SameImage(second.image, secondStock.image),
          "the next frame's pass shades no pixel it did not tag (stencil cleared at the pass start)");
    vf_test_set_config(&on);
}

void CheckUntaggedLiquidKeepsItsColour(BasinClient& client, const Config& base)
{
    const Config on = WaterConfig(base);
    Config off = on;
    off.water = false;
    WaterFrame covered;
    covered.covers = kCoverLiquids;
    covered.coverCount = 2;
    WaterFrame coverMask = covered;
    coverMask.calls = WaterCalls::None;
    coverMask.stripCount = 0;
    coverMask.opaqueMask = true;
    vf_test_set_config(&on);
    const WaterFrameResult mask = client.Render(coverMask);
    const WaterFrameResult shaded = client.Render(covered);
    vf_test_set_config(&off);
    const WaterFrameResult stock = client.Render(covered);
    vf_test_set_config(&on);
    const MaskComparison cover = CompareByMask(mask.image, shaded.image, stock.image);
    std::printf("     untagged liquid over tagged water: %zu pixels, %zu reshaded; %zu other pixels changed\n",
                cover.waterPixels, cover.changedWater, cover.changedElsewhere);
    Check(shaded.began && cover.waterPixels > 0 && cover.changedWater == 0 && cover.changedElsewhere > 0,
          "liquids drawn in the pass without a water class (hooked as None or not hooked) in front of tagged water "
          "keep their own colour");
}

void CheckClassesShadeSeparately(BasinClient& client, const Config& base, const std::wstring& outDir)
{
    Config classes = WaterConfig(base);
    classes.waterDebugView = 5;
    vf_test_set_config(&classes);
    WaterFrame frame;
    frame.strips = kClassStrips;
    frame.stripCount = 3;
    const WaterFrameResult shaded = client.Render(frame);
    classes.water = false;
    vf_test_set_config(&classes);
    const WaterFrameResult stock = client.Render(frame);
    SaveImage(outDir, L"water-classes", shaded.image);
    const WaterView& v = client.View();
    auto pixelOf = [&v](float x, float y) {
        const Vec3 d = Sub(Add({x, y, kWaterSurfaceZ}, kGameLikeWorldOffset), v.eye);
        const float* m = v.view;
        const float vx = d.x * m[0] + d.y * m[4] + d.z * m[8];
        const float vy = d.x * m[1] + d.y * m[5] + d.z * m[9];
        const float vz = d.x * m[2] + d.y * m[6] + d.z * m[10];
        const float ndcX = vx / vz * v.proj[0] + v.proj[8];
        const float ndcY = vy / vz * v.proj[5] + v.proj[9];
        return std::make_pair(static_cast<UINT>((ndcX * 0.5f + 0.5f) * v.world.Width),
                              static_cast<UINT>((0.5f - ndcY * 0.5f) * v.world.Height));
    };
    const auto lake = pixelOf(kClassProbeX, 150.0f);
    const auto ocean = pixelOf(kClassProbeX, 20.0f);
    const auto river = pixelOf(kClassProbeX, -150.0f);
    const unsigned char* lakeColour = shaded.image.At(lake.first, lake.second);
    const unsigned char* oceanColour = shaded.image.At(ocean.first, ocean.second);
    const bool lakeRed = lakeColour[2] == 255 && lakeColour[1] == 0 && lakeColour[0] == 0;
    const bool oceanBlue = oceanColour[2] == 0 && oceanColour[1] == 0 && oceanColour[0] == 255;
    const bool riverStock = std::memcmp(shaded.image.At(river.first, river.second),
                                        stock.image.At(river.first, river.second), 3) == 0;
    std::printf("     class view: lake %u,%u = %u %u %u, ocean %u,%u = %u %u %u, river kept %d\n", lake.first,
                lake.second, lakeColour[2], lakeColour[1], lakeColour[0], ocean.first, ocean.second, oceanColour[2],
                oceanColour[1], oceanColour[0], riverStock);
    Check(shaded.began && lakeRed && oceanBlue && riverStock,
          "each drawn class is shaded by its own stencil-tested pass; a class without a preset keeps its water");
}

void CheckReflectionsOffSelectTheSkyOnlyVariant(BasinClient& client, const Config& base)
{
    WaterFrame frame;
    bool selected = true;
    bool unchanged = true;
    for (int quality : kReflectingQualities)
    {
        Config cfg = WaterConfig(base);
        cfg.waterQuality = quality;
        cfg.waterReflections = 1.0f;
        vf_test_set_config(&cfg);
        const bool reflected = client.Render(frame).began;
        const int reflecting = vf_test_water_shading_variant();
        cfg.waterReflections = 0.0f;
        vf_test_set_config(&cfg);
        const WaterFrameResult skyOnly = client.Render(frame);
        const int withoutReflections = vf_test_water_shading_variant();
        vf_test_force_water_shading_variant(reflecting);
        const WaterFrameResult traced = client.Render(frame);
        vf_test_force_water_shading_variant(kNoForcedVariant);
        const bool same = SameImage(skyOnly.image, traced.image);
        std::printf("     WaterQuality %d: variant %d with reflections, %d without; image with the quality variant "
                    "unchanged %d\n",
                    quality, reflecting, withoutReflections, same);
        selected = selected && reflected && skyOnly.began && reflecting == quality - 1 &&
                   withoutReflections == kSkyReflectionsOnlyVariant;
        unchanged = unchanged && traced.began && same;
    }
    Check(selected, "WaterReflections=0 selects the variant without screen-space reflections at WaterQuality 2 and 3 "
                    "(the harness shows the selection, not the in-game saving)");
    Check(unchanged, "without reflections the sky-only variant shades the same image as the quality variant");
    const Config on = WaterConfig(base);
    vf_test_set_config(&on);
}

void CheckFlatFallback(BasinClient& client, const Config& base, const Image& stock, const std::wstring& outDir)
{
    vf_test_disable_wave_simulation(1);
    Config waves = WaterConfig(base);
    waves.waterWaves = 1.0f;
    vf_test_set_config(&waves);
    WaterFrame frame;
    const WaterFrameResult withWaves = client.Render(frame);
    Config flat = waves;
    flat.waterWaves = 0.0f;
    vf_test_set_config(&flat);
    const WaterFrameResult withoutWaves = client.Render(frame);
    vf_test_disable_wave_simulation(0);
    SaveImage(outDir, L"water-flat-fallback", withWaves.image);
    Check(withWaves.began && SameImage(withWaves.image, withoutWaves.image) && !SameImage(withWaves.image, stock),
          "without the wave simulation the water is shaded flat (1x1 zero maps) instead of skipped");
}

void CheckEndReportsWhatItShaded(Harness& h, BasinClient& client, const Config& base)
{
    const Config on = WaterConfig(base);
    vf_test_set_config(&on);
    WaterFrame lake;
    const WaterFrameResult drawn = client.Render(lake);
    IDirect3DSurface9* otherDepth = nullptr;
    const bool created = SUCCEEDED(h.dev->CreateDepthStencilSurface(h.pp.BackBufferWidth, h.pp.BackBufferHeight,
                                                                     D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE,
                                                                     &otherDepth, nullptr));
    WaterFrame rebound = lake;
    rebound.depthAtEnd = otherDepth;
    const WaterFrameResult unshaded = created ? client.Render(rebound) : WaterFrameResult();
    if (otherDepth)
        otherDepth->Release();
    const WaterStrip riverOnly[] = {{-kBasinHalfWidth, kBasinHalfWidth, kRiverClass, kStockWaterColour}};
    WaterFrame river;
    river.strips = riverOnly;
    const WaterFrameResult presetless = client.Render(river);
    std::printf("     End: lake %d, rebound depth %d (%s), river without a preset %d (%s)\n", drawn.shaded,
                unshaded.shaded, unshaded.endSkip, presetless.shaded, presetless.endSkip);
    Check(drawn.began && drawn.shaded && created && unshaded.began && !unshaded.shaded &&
              std::strcmp(unshaded.endSkip, "fog depth surface not bound") == 0 && unshaded.stateKept,
          "End reports that it shaded nothing, and why, when the fog depth is no longer bound");
    Check(presetless.began && !presetless.shaded &&
              std::strcmp(presetless.endSkip, "no water preset for the drawn classes") == 0,
          "End reports that it shaded nothing when no drawn class has a preset");
}

void CheckWaterGpuTimeSummary(BasinClient& client, const Config& base)
{
    Config logged = WaterConfig(base);
    logged.logLevel = static_cast<int>(LogLevel::Info);
    vf_test_set_config(&logged);
    WaterFrame frame;
    vf_test_force_water_summary();
    bool began = client.Render(frame).began;
    const size_t start = water_settings_checks::DllLogSize();
    for (int i = 0; i < kTimedWaterFrames; ++i)
        began = client.Render(frame).began && began;
    vf_test_force_water_summary();
    began = client.Render(frame).began && began;
    const std::string text = runtime_cost::LogWrittenSince(start);
    const size_t at = text.rfind("water gpu ");
    float medianMs = 0.0f;
    unsigned frames = 0;
    unsigned skipped = 0;
    char details[128] = {};
    const char* const format = "water gpu %f ms (median of %u frames, %u skipped), %127[^\r\n]";
    const bool parsed =
        at != std::string::npos && std::sscanf(text.c_str() + at, format, &medianMs, &frames, &skipped, details) == 4;
    std::printf("     water summary: %.3f ms over %u frames, %u skipped, %s\n", medianMs, frames, skipped, details);
    Check(began && parsed && frames > 0 && medianMs > 0.0f &&
              std::strcmp(details, "classes lake, waves 256 (3 tiles)") == 0,
          "the water summary reports the water pass's GPU time, the classes shaded and the wave simulation");
    const Config on = WaterConfig(base);
    vf_test_set_config(&on);
}

void CheckFaultInsideThePassRestoresTheDevice(Harness& h, BasinClient& client, const Config& base)
{
    const Config on = WaterConfig(base);
    vf_test_set_config(&on);
    IDirect3DSurface9* backBuffer = nullptr;
    IDirect3DSurface9* depth = nullptr;
    h.dev->GetRenderTarget(0, &backBuffer);
    h.dev->GetDepthStencilSurface(&depth);
    const ULONG backBufferReferences = References(backBuffer);
    const ULONG depthReferences = References(depth);
    const struct
    {
        int stage;
        const char* check;
    } faults[] = {
        {kFaultInBegin, "an exception while Begin has the device switched is aborted back to the client's targets, "
                        "state and references, and the fog still draws"},
        {kFaultInEnd, "an exception while End has the device switched is aborted back to the client's targets, "
                      "state and references, and the fog still draws"},
    };
    for (const auto& fault : faults)
    {
        WaterFrame frame;
        frame.fault = fault.stage;
        frame.fogDepthView = true;
        const WaterFrameResult result = client.Render(frame);
        const bool referencesKept =
            References(backBuffer) == backBufferReferences && References(depth) == depthReferences;
        std::printf("     fault %d: raised %d, state kept %d, references kept %d, fog drawn %d\n", fault.stage,
                    result.faulted, result.stateKept, referencesKept, result.fogRendered);
        Check(result.faulted && result.stateKept && referencesKept && result.fogRendered, fault.check);
    }
    backBuffer->Release();
    depth->Release();
    h.ReleaseEngineObjects();
    const HRESULT reset = h.dev->Reset(&h.pp);
    h.CreateEngineObjects();
    WaterFrame frame;
    const WaterFrameResult after = client.Render(frame);
    Check(SUCCEEDED(reset) && after.began && after.stateKept,
          "after an aborted water pass the device resets and the next water pass draws");
}

void CheckWaterOffReleasesResources(BasinClient& client, const Config& base)
{
    Config on = WaterConfig(base);
    vf_test_set_config(&on);
    WaterFrame frame;
    frame.calls = WaterCalls::Hooks;
    const WaterFrameResult before = client.Render(frame);
    const unsigned heldOn = vf_test_water_resources_held();
    const int maskPool = vf_test_water_mask_pool();
    Config off = on;
    off.water = false;
    vf_test_set_config(&off);
    const WaterFrameResult disabled = client.Render(frame);
    const unsigned heldOff = vf_test_water_resources_held();
    vf_test_set_config(&on);
    const WaterFrameResult after = client.Render(frame);
    std::printf("     water resources held: 0x%X on, 0x%X off; foam mask pool %d\n", heldOn, heldOff, maskPool);
    Check(before.began && heldOn == kAllWaterResources && !disabled.began && heldOff == 0,
          "turning water off releases the scene copies, wave maps and foam masks at the end of the next frame "
          "through the hooks the game runs");
    Check(after.began && SameImage(before.image, after.image),
          "turning water back on recreates them and renders the same frame");
    Check(maskPool == D3DPOOL_DEFAULT,
          "foam masks live in the default pool, without a managed system-memory copy beside the CPU one");
}

void CheckWaterStatusListsTheShadedClasses(BasinClient& client, const Config& base)
{
    const Config on = WaterConfig(base);
    vf_test_set_config(&on);
    WaterFrame classes;
    classes.calls = WaterCalls::Hooks;
    classes.strips = kClassStrips;
    classes.stripCount = 3;
    const WaterFrameResult result = client.Render(classes);
    const char* status = "";
    const bool drawn = vf_test_water_status(&status) != 0;
    std::printf("     settings window water status after lake, ocean and river draws: %s\n", status);
    Check(result.began && drawn && std::strcmp(status, "lake, ocean") == 0,
          "the settings window lists the water classes End shaded, not a drawn class without a preset");
}

void CheckFaultedFirstBeginReleasesItsResources(BasinClient& client, const Config& base)
{
    const Config on = WaterConfig(base);
    Config off = on;
    off.water = false;
    WaterFrame hooked;
    hooked.calls = WaterCalls::Hooks;
    vf_test_set_config(&off);
    client.Render(hooked);
    const unsigned released = vf_test_water_resources_held();
    vf_test_set_config(&on);
    WaterFrame faulted = hooked;
    faulted.fault = kFaultInBegin;
    const WaterFrameResult result = client.Render(faulted);
    const unsigned held = vf_test_water_resources_held();
    std::printf("     water resources held: 0x%X after the release, 0x%X after a faulted first Begin\n", released, held);
    Check(released == 0 && !result.began && result.stateKept && held == 0,
          "an exception in the first water Begin after a release stops the water and releases what that Begin "
          "created at the frame end");
}

void CheckFailedFoamMaskUploadsRetry(BasinClient& client, const Config& base)
{
    const Config on = WaterConfig(base);
    vf_test_set_config(&on);
    const SyntheticWaterData data = MakeSyntheticWaterData();
    const int maskCount = static_cast<int>(data.maskInfo.size());
    const bool assigned = AssignWaterData(data);
    vf_test_fail_water_mask_uploads(1);
    WaterFrame frame;
    const WaterFrameResult failed = client.Render(frame);
    const int afterFailure = vf_test_water_masks_uploaded();
    int passes = 0;
    int uploaded = afterFailure;
    while (uploaded < maskCount && passes < kMaxMaskRetryPasses)
    {
        client.Render(frame);
        ++passes;
        uploaded = vf_test_water_masks_uploaded();
    }
    vf_test_fail_water_mask_uploads(0);
    std::printf("     foam masks: %d of %d after a failed upload, %d after %d more water passes\n", afterFailure,
                maskCount, uploaded, passes);
    Check(assigned && failed.began && afterFailure == maskCount - 1 && passes > 1 && uploaded == maskCount,
          "a foam mask whose upload failed is uploaded again a few water passes later instead of staying flat "
          "until the next Reset");
}

SyntheticWaterData WaveFoamOnlyLake()
{
    SyntheticWaterData data = MakeSyntheticWaterData();
    for (WaterPreset& preset : data.presets)
    {
        Set4(preset.shoreFoam, 0.0f, 0.0f, 0.0f, 0.0f);
        Set4(preset.depthFadeFoam, 0.0f, 0.0f, 0.0f, 0.0f);
    }
    return data;
}

WaterFrameResult RenderAfterFoamClock(BasinClient& client, const WaterFrame& frame)
{
    for (int step = 1; step <= kFoamClockSteps; ++step)
    {
        vf_test_set_water_seconds(kFrameSeconds + kFoamClockStep * step);
        client.Render(frame);
    }
    return client.Render(frame);
}

void CheckFlatWaterHasNoCrestFoam(BasinClient& client, const Config& base)
{
    const bool assigned = AssignWaterData(WaveFoamOnlyLake());
    Config flat = WaterConfig(base);
    flat.waterWaves = 0.0f;
    flat.waterFoam = 1.0f;
    flat.waterDebugView = kFoamCoverageDebugView;
    vf_test_set_config(&flat);
    WaterFrame frame;
    const WaterFrameResult settled = RenderAfterFoamClock(client, frame);
    vf_test_disable_wave_simulation(1);
    const WaterFrameResult fallback = client.Render(frame);
    vf_test_disable_wave_simulation(0);
    vf_test_set_water_seconds(kFrameSeconds);
    Check(assigned && settled.began && SameImage(settled.image, fallback.image),
          "WaterWaves=0 shades flat water without crest foam, as without the wave simulation");
    const Config on = WaterConfig(base);
    vf_test_set_config(&on);
    AssignWaterData(MakeSyntheticWaterData());
}

std::pair<UINT, UINT> WaterSurfacePixel(const WaterView& v, float x, float y)
{
    const Vec3 d = Sub(Add({x, y, kWaterSurfaceZ}, kGameLikeWorldOffset), v.eye);
    const float* m = v.view;
    const float vx = d.x * m[0] + d.y * m[4] + d.z * m[8];
    const float vy = d.x * m[1] + d.y * m[5] + d.z * m[9];
    const float vz = d.x * m[2] + d.y * m[6] + d.z * m[10];
    const float ndcX = vx / vz * v.proj[0] + v.proj[8];
    const float ndcY = vy / vz * v.proj[5] + v.proj[9];
    return {static_cast<UINT>((ndcX * 0.5f + 0.5f) * v.world.Width),
            static_cast<UINT>((0.5f - ndcY * 0.5f) * v.world.Height)};
}

void CheckShoreFoamWidthFollowsTheSlope(Harness& h, const Config& base)
{
    SyntheticWaterData data = MakeSyntheticWaterData();
    WaterPreset& lake = data.presets[0];
    Set4(lake.depthFadeFoam, 0.0f, 0.0f, 0.0f, 0.0f);
    Set4(lake.waveFoam, 0.0f, 0.0f, 0.0f, 0.0f);
    Set4(lake.shoreFoam, 1.0f, 18.0f, kShoreFoamFadeDepth, 0.0f);
    lake.masks[static_cast<int>(WaterMaskSlot::ShoreFoam)] = 0;
    const bool assigned = AssignWaterData(data);
    Config coverage = WaterConfig(base);
    coverage.waterDebugView = kFoamCoverageDebugView;
    vf_test_set_config(&coverage);
    vf_test_disable_wave_simulation(1);
    const WaterView view = MakeWaterView({360, 0, 40}, {398, 0, -3});
    BasinClient client(h, view);
    WaterFrame frame;
    const WaterFrameResult result = client.Render(frame);
    vf_test_disable_wave_simulation(0);
    const auto nearShore = WaterSurfacePixel(view, kBeachWaterlineX - kNearShoreDepth / kBeachRisePerYard, 0.0f);
    const auto midShore = WaterSurfacePixel(view, kBeachWaterlineX - kMidShoreDepth / kBeachRisePerYard, 0.0f);
    const BYTE nearCoverage = result.image.At(nearShore.first, nearShore.second)[1];
    const BYTE midCoverage = result.image.At(midShore.first, midShore.second)[1];
    std::printf("     shore foam coverage: %u at %.2f yd of water (pixel %u,%u), %u at %.2f yd (pixel %u,%u)\n",
                nearCoverage, kNearShoreDepth, nearShore.first, nearShore.second, midCoverage, kMidShoreDepth,
                midShore.first, midShore.second);
    Check(assigned && result.began && nearCoverage >= kVisibleFoamCoverage && midCoverage <= kNoFoamCoverage,
          "shore foam fades over the estimated distance from the shore (about four times the water depth), not over "
          "the vertical depth");
    const Config on = WaterConfig(base);
    vf_test_set_config(&on);
    AssignWaterData(MakeSyntheticWaterData());
}

SyntheticWaterData RoughWater()
{
    SyntheticWaterData rough = MakeSyntheticWaterData();
    for (WaterFftTile& tile : rough.tiles)
    {
        tile.amplitude = kRoughTileAmplitude;
        tile.windMultiplier = kRoughTileWindMultiplier;
    }
    return rough;
}

int SlopeDetail(const Image& normals, UINT x, UINT y)
{
    const unsigned char* p = normals.At(x, y);
    return std::abs(p[2] - 128) + std::abs(p[1] - 128);
}

void CheckOccluderEdgesKeepWaveDetail(Harness& h, const Config& base)
{
    const bool assigned = AssignWaterData(RoughWater());
    const WaterView view = MakeWaterView({95, -37, 2.5f}, {140, -37, 0});
    BasinClient client(h, view);
    Config normals = WaterConfig(base);
    normals.waterDebugView = kNormalDebugView;
    vf_test_set_config(&normals);
    WaterFrame mask;
    mask.calls = WaterCalls::None;
    mask.opaqueMask = true;
    const WaterFrameResult water = client.Render(mask);
    WaterFrame pass;
    const WaterFrameResult shaded = client.Render(pass);
    SaveImage(g_harnessOutDir, L"water-occluder-normals", shaded.image);
    UINT left = view.world.Width;
    UINT right = 0;
    for (float x : {kOccludedBoxLow.x, kOccludedBoxHigh.x})
        for (float y : {kOccludedBoxLow.y, kOccludedBoxHigh.y})
        {
            const UINT column = WaterSurfacePixel(view, x, y).first;
            left = std::min(left, column);
            right = std::max(right, column);
        }
    size_t edges = 0;
    double edgeDetail = 0.0;
    double referenceDetail = 0.0;
    const UINT first = left > kEdgeSearchMargin + kEdgeReferenceOffset ? left - kEdgeSearchMargin : 0;
    const UINT last = std::min<UINT>(right + kEdgeSearchMargin, view.world.Width - kEdgeReferenceOffset - 2);
    for (UINT y = 0; y < view.world.Height; ++y)
        for (UINT x = std::max(first, static_cast<UINT>(kEdgeReferenceOffset)); x <= last; ++x)
        {
            const UINT partner = x ^ 1u;
            if (!IsMaskPixel(water.image, x, y) || IsMaskPixel(water.image, partner, y))
                continue;
            const UINT reference = partner > x ? x - kEdgeReferenceOffset : x + kEdgeReferenceOffset;
            if (!IsMaskPixel(water.image, reference, y) || !IsMaskPixel(water.image, reference ^ 1u, y))
                continue;
            ++edges;
            edgeDetail += SlopeDetail(shaded.image, x, y);
            referenceDetail += SlopeDetail(shaded.image, reference, y);
        }
    const double edgeMean = edges ? edgeDetail / edges : 0.0;
    const double referenceMean = edges ? referenceDetail / edges : 0.0;
    std::printf("     occluder edges: %zu water pixels beside the box, slope detail %.1f at the edge, %.1f %d px off\n",
                edges, edgeMean, referenceMean, kEdgeReferenceOffset);
    Check(assigned && shaded.began && edges >= kMinOccluderEdgePixels && referenceMean >= kMinReferenceSlopeDetail &&
              edgeMean >= kMinEdgeDetailRatio * referenceMean,
          "water beside an occluder keeps its wave detail (texture gradients follow the water plane, not the "
          "occluder's depth)");
    const Config on = WaterConfig(base);
    vf_test_set_config(&on);
    AssignWaterData(MakeSyntheticWaterData());
}

bool NearColour(const unsigned char* bgr, DWORD colour, int tolerance)
{
    for (int channel = 0; channel < 3; ++channel)
        if (std::abs(bgr[channel] - static_cast<int>((colour >> (8 * channel)) & 0xFF)) > tolerance)
            return false;
    return true;
}

int ColourDistance(const unsigned char* a, const double* b)
{
    return static_cast<int>(std::fabs(a[0] - b[0]) + std::fabs(a[1] - b[1]) + std::fabs(a[2] - b[2]));
}

void CheckReflectionsCarryTheirSourcesFog(Harness& h, const Config& base)
{
    BasinClient client(h, DefaultWaterView());
    Config clear = WaterConfig(base);
    clear.density = 0.0f;
    Config fogged = WaterConfig(base);
    WaterFrame stock;
    stock.calls = WaterCalls::None;
    WaterFrame mask = stock;
    mask.opaqueMask = true;
    WaterFrame reflection;
    WaterFrame composited;
    composited.fogDepthView = true;
    vf_test_set_config(&clear);
    const WaterFrameResult unfoggedScene = client.Render(stock);
    const WaterFrameResult water = client.Render(mask);
    clear.waterDebugView = kReflectionDebugView;
    vf_test_set_config(&clear);
    const WaterFrameResult clearReflection = client.Render(reflection);
    vf_test_set_config(&fogged);
    const WaterFrameResult foggedScene = client.Render(composited);
    SaveImage(g_harnessOutDir, L"water-fogged-frame", foggedScene.image);
    fogged.waterDebugView = kReflectionDebugView;
    vf_test_set_config(&fogged);
    const WaterFrameResult foggedReflection = client.Render(reflection);
    double wall[3] = {};
    size_t wallPixels = 0;
    for (UINT y = 0; y < water.image.h; ++y)
        for (UINT x = 0; x < water.image.w; ++x)
            if (NearColour(unfoggedScene.image.At(x, y), kFarWallColour, 0))
            {
                for (int channel = 0; channel < 3; ++channel)
                    wall[channel] += foggedScene.image.At(x, y)[channel];
                ++wallPixels;
            }
    for (double& channel : wall)
        channel /= std::max<size_t>(wallPixels, 1);
    size_t reflections = 0;
    double clearDistance = 0.0;
    double foggedDistance = 0.0;
    double change = 0.0;
    for (UINT y = 0; y < water.image.h; ++y)
        for (UINT x = 0; x < water.image.w; ++x)
        {
            const unsigned char* before = clearReflection.image.At(x, y);
            if (!IsMaskPixel(water.image, x, y) || !NearColour(before, kFarWallColour, kReflectionMatchTolerance))
                continue;
            const unsigned char* after = foggedReflection.image.At(x, y);
            ++reflections;
            clearDistance += ColourDistance(before, wall);
            foggedDistance += ColourDistance(after, wall);
            change += std::abs(after[0] - before[0]) + std::abs(after[1] - before[1]) + std::abs(after[2] - before[2]);
        }
    const double count = static_cast<double>(std::max<size_t>(reflections, 1));
    std::printf("     far wall: %zu pixels, fogged %.0f %.0f %.0f; %zu water pixels reflect it, distance to the fogged "
                "wall %.1f clear, %.1f fogged\n",
                wallPixels, wall[2], wall[1], wall[0], reflections, clearDistance / count, foggedDistance / count);
    Check(wallPixels > 0 && reflections >= kMinWallReflectionPixels && change / count >= kMinReflectionFogChange &&
              foggedDistance < clearDistance,
          "with the fog replacing the stock fog, a reflection is fogged along its reflected path toward its fogged "
          "source");
    const Config on = WaterConfig(base);
    vf_test_set_config(&on);
}

UINT RowAtElevation(const WaterView& v, UINT column, float elevation)
{
    UINT best = 0;
    float bestError = std::numeric_limits<float>::max();
    for (UINT row = 0; row < v.world.Height; ++row)
    {
        const Vec3 ray = PixelRay(v, column + 0.5f, row + 0.5f);
        const float error = std::fabs(std::atan2(ray.z, std::sqrt(ray.x * ray.x + ray.y * ray.y)) - elevation);
        if (error < bestError)
        {
            best = row;
            bestError = error;
        }
    }
    return best;
}

void CheckSkyReflectionsCarryTheSkysFog(Harness& h, const Config& base)
{
    WaterView view = MakeWaterView(kLowEye, kLowEyeTarget);
    for (uint32_t& sky : view.water.skyColors)
        sky = kBlackSky;
    BasinClient client(h, view);
    Config heightFog = WaterConfig(base);
    heightFog.dataMode = 0;
    heightFog.farFog = 0.0f;
    heightFog.waterQuality = 1;
    heightFog.waterWaves = 0.0f;
    heightFog.waterDebugView = kReflectionDebugView;
    vf_test_set_config(&heightFog);
    WaterFrame reflection;
    const WaterFrameResult mirrored = client.Render(reflection);
    Config skyFog = heightFog;
    skyFog.debugView = kFogRadianceDebugView;
    skyFog.waterDebugView = 0;
    vf_test_set_config(&skyFog);
    WaterFrame direct;
    direct.calls = WaterCalls::None;
    direct.fogDepthView = true;
    const WaterFrameResult sky = client.Render(direct);
    const UINT column = view.world.Width / 2;
    float worst = 0.0f;
    for (float degrees : kSkyFogElevationsDegrees)
    {
        const float elevation = degrees * kPi / 180.0f;
        const UINT skyRow = RowAtElevation(view, column, elevation);
        const UINT waterRow = RowAtElevation(view, column, -elevation);
        const float direct = Decode(sky.image.At(column, skyRow)[1]);
        const float reflected = Decode(mirrored.image.At(column, waterRow)[1]);
        const float mismatch = std::fabs(reflected / std::fmax(direct, kMinComparedFogRadiance) - 1.0f);
        std::printf("     %4.1f degrees: sky row %u fog %.4f, reflected at row %u %.4f (%+.0f%%)\n", degrees, skyRow,
                    direct, waterRow, reflected, (reflected / std::fmax(direct, kMinComparedFogRadiance) - 1) * 100);
        worst = std::fmax(worst, mismatch);
    }
    Check(mirrored.began && sky.fogRendered && worst <= kMaxSkyReflectionFogMismatch,
          "a sky reflection carries the height fog the fog pass puts on the sky at the same elevation");
    const Config on = WaterConfig(base);
    vf_test_set_config(&on);
}

void CheckResetKeepsWater(Harness& h, BasinClient& client, const Config& base)
{
    vf_test_disable_wave_simulation(1);
    const Config on = WaterConfig(base);
    vf_test_set_config(&on);
    WaterFrame frame;
    const WaterFrameResult before = client.Render(frame);
    h.ReleaseEngineObjects();
    const HRESULT reset = h.dev->Reset(&h.pp);
    h.CreateEngineObjects();
    const WaterFrameResult after = client.Render(frame);
    vf_test_disable_wave_simulation(0);
    Check(SUCCEEDED(reset) && before.began && after.began && after.stateKept && SameImage(before.image, after.image),
          "Reset with water resources alive, then the water pass renders the same frame again");
}

void CheckStockFogOnWater(Harness& h, const Config& base)
{
    WaterView view = DefaultWaterView();
    view.water.stockFogApplies = true;
    view.in.fogStart = kNearStockFogStart;
    view.in.fogEnd = kNearStockFogEnd;
    view.in.fogColor = kStockFogColour;
    const std::vector<AbsorptionSample> samples = FlatFloorSamples(view);
    const Config on = WaterConfig(base);
    vf_test_set_config(&on);
    BasinClient fogged(h, view);
    WaterFrame frame;
    const WaterFrameResult result = fogged.Render(frame);
    view.water.stockFogApplies = false;
    BasinClient clear(h, view);
    const WaterFrameResult unfogged = clear.Render(frame);
    bool matches = !samples.empty();
    bool differs = false;
    for (const AbsorptionSample& sample : samples)
    {
        const unsigned char* got = result.image.At(sample.x, sample.y);
        const unsigned char* plain = unfogged.image.At(sample.x, sample.y);
        const int expected[3] = {kStockFogColour & 0xFF, (kStockFogColour >> 8) & 0xFF, (kStockFogColour >> 16) & 0xFF};
        for (int channel = 0; channel < 3; ++channel)
        {
            matches = matches && std::abs(got[channel] - expected[channel]) <= 1;
            differs = differs || plain[channel] != got[channel];
        }
    }
    Check(result.began && matches && differs,
          "where the client's stock fog applies, water beyond its end takes the stock fog colour");
}

float StockFogVisibilityAt(float viewZ)
{
    return std::fmin(std::fmax((kShoreStockFogEnd - viewZ) / (kShoreStockFogEnd - kShoreStockFogStart), 0.0f), 1.0f);
}

void CheckStockFoggedCopyIsFoggedOnce(Harness& h, const Config& base)
{
    SyntheticWaterData data = MakeSyntheticWaterData();
    for (WaterPreset& preset : data.presets)
        Set4(preset.scatteringIntensities, 0.0f, 0.0f, 0.0f, 0.0f);
    const bool assigned = AssignWaterData(data);
    const Config optics = OpticsOnlyConfig(base);
    vf_test_set_config(&optics);
    WaterView view = DefaultWaterView();
    view.water.stockFogApplies = true;
    view.sceneStockFogged = true;
    view.in.fogStart = kShoreStockFogStart;
    view.in.fogEnd = kShoreStockFogEnd;
    view.in.fogColor = kShoreStockFogColour;
    BasinClient client(h, view);
    WaterFrame frame;
    const WaterFrameResult fogged = client.Render(frame);
    const std::vector<AbsorptionSample> samples = FlatFloorSamples(view);
    const WaterPreset& lake = data.presets[0];
    float worst = samples.empty() ? 1.0f : 0.0f;
    for (const AbsorptionSample& s : samples)
    {
        const unsigned char* got = fogged.image.At(s.x, s.y);
        const float waterVisibility = StockFogVisibilityAt(s.waterZ);
        for (int channel = 0; channel < 3; ++channel)
        {
            const int rgb = 2 - channel;
            const auto floor = static_cast<unsigned char>((kFloorColour >> (8 * channel)) & 0xFF);
            const float fogEncoded = ((kShoreStockFogColour >> (8 * channel)) & 0xFF) / 255.0f;
            const float path = s.floorZ - s.waterZ;
            const float water = Encode(Decode(floor) * std::exp(-lake.absorption[rgb] * lake.absorption[3] * path));
            const float expected = fogEncoded + waterVisibility * (water - fogEncoded);
            worst = std::fmax(worst, std::fabs(got[channel] / 255.0f - expected));
        }
    }
    std::printf("     stock-fogged scene copy: %zu samples, worst error %.2f/255 against fogging the water once\n",
                samples.size(), worst * 255.0f);
    Check(assigned && fogged.began && !samples.empty() && worst <= kFoggedCopyTolerance,
          "where the client's stock fog is in the scene copy, refracted water is fogged once, at the water surface");
    const Config on = WaterConfig(base);
    vf_test_set_config(&on);
    AssignWaterData(MakeSyntheticWaterData());
}

void CheckInteriorIgnoresSun(Harness& h, const Config& base)
{
    const Config on = WaterConfig(base);
    vf_test_set_config(&on);
    WaterView sunny = DefaultWaterView();
    WaterView otherSun = sunny;
    otherSun.in.directColor = kOtherSunColour;
    otherSun.in.toLight[0] = 0.0f;
    otherSun.in.toLight[1] = 0.6f;
    otherSun.in.toLight[2] = 0.8f;
    WaterFrame interior;
    interior.strips = kInteriorBasin;
    WaterFrame lake;
    BasinClient first(h, sunny);
    BasinClient second(h, otherSun);
    const WaterFrameResult interiorA = first.Render(interior);
    const WaterFrameResult interiorB = second.Render(interior);
    const WaterFrameResult lakeA = first.Render(lake);
    const WaterFrameResult lakeB = second.Render(lake);
    Check(interiorA.began && SameImage(interiorA.image, interiorB.image) && !SameImage(lakeA.image, lakeB.image),
          "interior water ignores the sun (no sun scattering, glint or foam light) while lake water follows it");
}

void CheckWaterFollowsTheDirectLight(Harness& h, const Config& base)
{
    const Config on = WaterConfig(base);
    vf_test_set_config(&on);
    WaterView view = DefaultWaterView();
    WaterView otherSprite = view;
    otherSprite.in.sunColor = kOtherSunColour;
    WaterView otherDirect = view;
    otherDirect.in.directColor = kOtherSunColour;
    WaterFrame lake;
    BasinClient reference(h, view);
    BasinClient sprite(h, otherSprite);
    BasinClient direct(h, otherDirect);
    const WaterFrameResult a = reference.Render(lake);
    const WaterFrameResult b = sprite.Render(lake);
    const WaterFrameResult c = direct.Render(lake);
    Check(a.began && SameImage(a.image, b.image) && !SameImage(a.image, c.image),
          "water is lit by the client's direct light (band 0), not by the sun sprite colour (band 9)");
}

bool MatchesColour(const unsigned char* bgr, DWORD colour)
{
    const int expected[3] = {static_cast<int>(colour & 0xFF), static_cast<int>((colour >> 8) & 0xFF),
                             static_cast<int>((colour >> 16) & 0xFF)};
    for (int channel = 0; channel < 3; ++channel)
        if (std::abs(bgr[channel] - expected[channel]) > kShoreColourTolerance)
            return false;
    return true;
}

void CheckGrazingReflectionsMissTheShore(Harness& h, const Config& base, const std::wstring& outDir)
{
    SyntheticWaterData rough = MakeSyntheticWaterData();
    for (WaterFftTile& tile : rough.tiles)
    {
        tile.amplitude = kRoughTileAmplitude;
        tile.windMultiplier = kRoughTileWindMultiplier;
    }
    const bool assigned = AssignWaterData(rough);
    BasinClient client(h, MakeWaterView(kGrazingEye, kGrazingTarget));
    Config reflection = WaterConfig(base);
    reflection.waterDebugView = kReflectionDebugView;
    reflection.density = 0.0f;
    vf_test_set_config(&reflection);
    WaterFrame mask;
    mask.calls = WaterCalls::None;
    mask.opaqueMask = true;
    const WaterFrameResult water = client.Render(mask);
    WaterFrame pass;
    const WaterFrameResult reflected = client.Render(pass);
    SaveImage(outDir, L"water-grazing-reflection", reflected.image);
    size_t considered = 0;
    size_t shore = 0;
    for (UINT x = 0; x < water.image.w; ++x)
    {
        UINT y = 0;
        while (y < water.image.h && !IsMaskPixel(water.image, x, y))
            ++y;
        for (y += kMirroredShoreRows; y < water.image.h && IsMaskPixel(water.image, x, y); ++y)
        {
            const unsigned char* colour = reflected.image.At(x, y);
            ++considered;
            shore += MatchesColour(colour, kLandColour) || MatchesColour(colour, kFloorColour) ? 1 : 0;
        }
    }
    const float fraction = considered ? static_cast<float>(shore) / static_cast<float>(considered) : 1.0f;
    std::printf("     grazing reflections: %zu of %zu water pixels below the mirrored shore reflect it (%.2f%%)\n",
                shore, considered, fraction * 100.0f);
    Check(assigned && reflected.began && considered > 0 && fraction <= kMaxShoreReflectionFraction,
          "wave facets that reflect below the horizon do not streak the far shore across distant water");
    const Config on = WaterConfig(base);
    vf_test_set_config(&on);
    AssignWaterData(MakeSyntheticWaterData());
}

SyntheticWaterData FoamCoveredLake(float tint)
{
    SyntheticWaterData data = MakeSyntheticWaterData();
    WaterPreset& lake = data.presets[0];
    Set4(lake.scatteringIntensities, 0.0f, 0.0f, 0.0f, 0.0f);
    Set4(lake.depthFadeFoam, 1.0f, 24.0f, kFoamEverywhereRange, 0.0f);
    Set4(lake.shoreFoam, 0.0f, 0.0f, 0.0f, 0.0f);
    Set4(lake.waveFoam, 0.0f, 0.0f, 0.0f, 0.0f);
    for (WaterMask& info : data.maskInfo)
        for (int c = 0; c < 3; ++c)
            info.tintLow[c] = info.tintHigh[c] = tint;
    return data;
}

void CheckFoamTintsAreLinear(BasinClient& client, const Config& base)
{
    Config foamOnly = WaterConfig(base);
    foamOnly.waterWaves = 0.0f;
    foamOnly.waterReflections = 0.0f;
    foamOnly.waterSpecular = 0.0f;
    foamOnly.waterFoam = 1.0f;
    vf_test_set_config(&foamOnly);
    vf_test_disable_wave_simulation(1);
    WaterFrame frame;
    const bool whiteAssigned = AssignWaterData(FoamCoveredLake(1.0f));
    const WaterFrameResult white = client.Render(frame);
    const bool dimAssigned = AssignWaterData(FoamCoveredLake(kDimFoamTint));
    const WaterFrameResult dim = client.Render(frame);
    vf_test_disable_wave_simulation(0);
    const std::vector<AbsorptionSample> samples = FlatFloorSamples(client.View());
    float lowest = 1.0f;
    float highest = 0.0f;
    for (const AbsorptionSample& s : samples)
        for (int channel = 0; channel < 3; ++channel)
        {
            const float ratio = Decode(dim.image.At(s.x, s.y)[channel]) /
                                std::fmax(Decode(white.image.At(s.x, s.y)[channel]), kMinDecodedFoam);
            lowest = std::fmin(lowest, ratio);
            highest = std::fmax(highest, ratio);
        }
    std::printf("     foam tinted %.2f against white: linear ratio %.3f..%.3f over %zu samples\n", kDimFoamTint, lowest,
                highest, samples.size());
    Check(whiteAssigned && dimAssigned && white.began && dim.began && !samples.empty() &&
              std::fabs(lowest - kDimFoamTint) <= kFoamTintTolerance &&
              std::fabs(highest - kDimFoamTint) <= kFoamTintTolerance,
          "foam mask tints are applied as the linear colours the water data stores");
    const Config on = WaterConfig(base);
    vf_test_set_config(&on);
    AssignWaterData(MakeSyntheticWaterData());
}

void LinearSkyAmbient(const WaterInputs& water, float* ambient)
{
    for (int c = 0; c < 3; ++c)
        ambient[c] = 0.0f;
    for (uint32_t sky : water.skyColors)
        for (int c = 0; c < 3; ++c)
            ambient[c] += Decode(static_cast<unsigned char>(sky >> (16 - 8 * c))) / kSkyColorCount;
}

void CheckSunsetFoamStaysBelowLitGround(Harness& h, const Config& base)
{
    Config foamOnly = WaterConfig(base);
    foamOnly.waterWaves = 0.0f;
    foamOnly.waterReflections = 0.0f;
    foamOnly.waterSpecular = 0.0f;
    foamOnly.waterFoam = 1.0f;
    vf_test_set_config(&foamOnly);
    vf_test_disable_wave_simulation(1);
    const bool assigned = AssignWaterData(FoamCoveredLake(1.0f));
    WaterView sunset = DefaultWaterView();
    const float horizontal = std::sqrt(1.0f - kSunsetElevation * kSunsetElevation);
    sunset.in.toLight[0] = horizontal;
    sunset.in.toLight[1] = 0.0f;
    sunset.in.toLight[2] = kSunsetElevation;
    sunset.in.lightIsMoon = false;
    sunset.in.directColor = kSunsetDirectColour;
    sunset.in.sunColor = kSunsetSpriteColour;
    BasinClient client(h, sunset);
    WaterFrame frame;
    const WaterFrameResult foam = client.Render(frame);
    vf_test_disable_wave_simulation(0);
    float ambient[3];
    LinearSkyAmbient(sunset.water, ambient);
    float litGround[3];
    for (int c = 0; c < 3; ++c)
        litGround[c] = ambient[c] + Decode(static_cast<unsigned char>(kSunsetDirectColour >> (16 - 8 * c))) *
                                        kSunsetElevation;
    const std::vector<AbsorptionSample> samples = FlatFloorSamples(sunset);
    float worstExcess = -1.0f;
    for (const AbsorptionSample& s : samples)
        for (int c = 0; c < 3; ++c)
        {
            const float encoded = foam.image.At(s.x, s.y)[2 - c] / 255.0f;
            worstExcess = std::fmax(worstExcess, encoded - Encode(litGround[c]));
        }
    std::printf("     sunset foam: white foam against white ground lit by the same light, worst excess %.1f/255\n",
                worstExcess * 255.0f);
    Check(assigned && foam.began && !samples.empty() && worstExcess <= kLitGroundTolerance,
          "a low warm sun does not make foam brighter than white ground lit by the same sun and sky");
    const Config on = WaterConfig(base);
    vf_test_set_config(&on);
    AssignWaterData(MakeSyntheticWaterData());
}

double MaskedCoverage(const Image& mask, const Image& coverage)
{
    double sum = 0.0;
    for (UINT y = 0; y < mask.h; ++y)
        for (UINT x = 0; x < mask.w; ++x)
            if (IsMaskPixel(mask, x, y))
                sum += coverage.At(x, y)[1];
    return sum;
}

struct CrestFoamResult
{
    bool rendered = false;
    MaskComparison shaded;
    double coverageBefore = 0.0;
    double coverageAfter = 0.0;
};

CrestFoamResult AccumulateCrestFoam(BasinClient& client, const Config& shading, const WaterFrame& frame,
                                    const Image& mask)
{
    Config coverage = shading;
    coverage.waterDebugView = kFoamCoverageDebugView;
    CrestFoamResult result;
    vf_test_set_water_seconds(kFrameSeconds);
    vf_test_set_config(&shading);
    const WaterFrameResult before = client.Render(frame);
    vf_test_set_config(&coverage);
    const WaterFrameResult beforeCoverage = client.Render(frame);
    vf_test_set_config(&shading);
    RenderAfterFoamClock(client, frame);
    vf_test_set_water_seconds(kFrameSeconds);
    const WaterFrameResult after = client.Render(frame);
    vf_test_set_config(&coverage);
    const WaterFrameResult afterCoverage = client.Render(frame);
    result.rendered = before.began && after.began && beforeCoverage.began && afterCoverage.began;
    result.shaded = CompareByMask(mask, after.image, before.image);
    result.coverageBefore = MaskedCoverage(mask, beforeCoverage.image);
    result.coverageAfter = MaskedCoverage(mask, afterCoverage.image);
    return result;
}

void CheckCrestFoamAccumulates(BasinClient& client, const Config& base, const Image& mask)
{
    const bool assigned = AssignWaterData(WaveFoamOnlyLake());
    WaterFrame frame;
    const CrestFoamResult foam = AccumulateCrestFoam(client, WaterConfig(base), frame, mask);
    std::printf("     crest foam over %d steps of %.1f s: %zu water pixels changed, %zu other pixels, coverage "
                "%.0f -> %.0f\n",
                kFoamClockSteps, kFoamClockStep, foam.shaded.changedWater, foam.shaded.changedElsewhere,
                foam.coverageBefore, foam.coverageAfter);
    Check(assigned && foam.rendered && foam.shaded.changedWater > 0 && foam.shaded.changedElsewhere == 0 &&
              foam.coverageAfter > foam.coverageBefore,
          "the simulated crest foam and oxygen accumulate over time and shade only the water");
    const Config on = WaterConfig(base);
    vf_test_set_config(&on);
    AssignWaterData(MakeSyntheticWaterData());
}

void CheckRealDataViews(Harness& h, const Config& base, const std::string& waterDataPath, const std::wstring& outDir)
{
    const bool loaded = vf_test_load_water_data(waterDataPath.c_str()) != 0;
    Check(loaded, "the converted water data loads for the real-data views");
    const Config cfg = WaterConfig(base);
    const struct
    {
        const wchar_t* name;
        Vec3 eye;
        Vec3 at;
        const WaterStrip* strips;
    } views[] = {
        {L"water-real-lake-low", {0, 0, 4}, {100, 5, 1}, kLakeBasin},
        {L"water-real-lake-high", {-20, 0, 40}, {150, -20, -10}, kLakeBasin},
        {L"water-real-lake-sun", {0, 0, 9}, {100, 12, 6}, kLakeBasin},
        {L"water-real-classes", {0, 0, 12}, {140, 0, 0}, kClassStrips},
    };
    for (const auto& view : views)
    {
        vf_test_set_config(&cfg);
        BasinClient client(h, MakeWaterView(view.eye, view.at));
        WaterFrame frame;
        frame.strips = view.strips;
        frame.stripCount = view.strips == kClassStrips ? 3 : 1;
        WaterFrame none = frame;
        none.calls = WaterCalls::None;
        WaterFrame mask = none;
        mask.opaqueMask = true;
        const WaterFrameResult stock = client.Render(none);
        const WaterFrameResult water = client.Render(mask);
        const WaterFrameResult result = client.Render(frame);
        SaveImage(outDir, view.name, result.image);
        const MaskComparison changes = CompareByMask(water.image, result.image, stock.image);
        std::printf("     %ls: %s%s, water pixels %zu, shaded %zu, changed elsewhere %zu\n", view.name,
                    result.began ? "drawn" : "skipped: ", result.skip, changes.waterPixels, changes.changedWater,
                    changes.changedElsewhere);
        const std::string check = "the real data shades the water of " + NarrowPath(view.name) +
                                  " and leaves every other pixel alone";
        Check(loaded && result.began && result.stateKept && changes.waterPixels > 0 &&
                  changes.changedWater >= kMinShadedFraction * changes.waterPixels && changes.changedElsewhere == 0,
              check.c_str());
    }
    BasinClient client(h, DefaultWaterView());
    WaterFrame none;
    none.calls = WaterCalls::None;
    none.opaqueMask = true;
    vf_test_set_config(&cfg);
    const WaterFrameResult mask = client.Render(none);
    Config windy = cfg;
    windy.waterWind = kWhitecapWind;
    WaterFrame frame;
    const CrestFoamResult foam = AccumulateCrestFoam(client, windy, frame, mask.image);
    std::printf("     real data at wind %.0f: crest foam coverage %.0f -> %.0f, %zu water pixels changed\n",
                kWhitecapWind, foam.coverageBefore, foam.coverageAfter, foam.shaded.changedWater);
    Check(loaded && foam.rendered && foam.coverageAfter > foam.coverageBefore && foam.shaded.changedElsewhere == 0,
          "the real presets, tiles and masks shade crest foam that builds up in a strong wind");
    const Config on = WaterConfig(base);
    vf_test_set_config(&on);
    Check(AssignWaterData(MakeSyntheticWaterData()), "synthetic water data restored after the real-data views");
}

void CheckWaterPass(Harness& h, const std::wstring& outDir, const std::string& waterDataPath)
{
    g_harnessOutDir = outDir;
    Config base;
    vf_test_get_config(&base);
    vf_test_set_water_seconds(kFrameSeconds);
    CheckQueuedTransparentLiquidsDecideArming();
    CheckWaterPassThunkKeepsTheRenderer();
    CheckIdleTagLeavesState(h);
    BasinClient client(h, DefaultWaterView());
    Check(AssignWaterData(MakeSyntheticWaterData()), "synthetic water presets, tiles and masks assigned");

    const Config on = WaterConfig(base);
    vf_test_set_config(&on);
    WaterFrame none;
    none.calls = WaterCalls::None;
    const WaterFrameResult stock = client.Render(none);
    WaterFrame mask = none;
    mask.opaqueMask = true;
    const WaterFrameResult waterMask = client.Render(mask);
    SaveImage(outDir, L"water-stock", stock.image);

    WaterFrame pass;
    const WaterFrameResult shaded = client.Render(pass);
    SaveImage(outDir, L"water-shaded", shaded.image);
    Check(shaded.began, (std::string("the water pass arms ") + shaded.skip).c_str());
    Check(shaded.stateKept, "render, stencil, sampler (s0-s15), shader, constant, stream, viewport, scissor and target "
                            "state is identical before the water pass and after it");
    Check(shaded.armWritesDocumented, "arming the water pass turns on stencil writes of 0 for every draw in the pass "
                                      "and changes no other state");
    Check(shaded.tagWritesDocumented, "tagging a water draw changes only the stencil reference, to its class");
    Check(shaded.untagRestores, "untagging returns the stencil reference to 0 and changes nothing else");
    const MaskComparison changes = CompareByMask(waterMask.image, shaded.image, stock.image);
    std::printf("     water pixels %zu, shaded %zu, changed outside the water %zu\n", changes.waterPixels,
                changes.changedWater, changes.changedElsewhere);
    Check(changes.waterPixels > 0 && changes.changedElsewhere == 0,
          "only tagged water pixels change; the other pass and all non-water pixels are bit-identical");
    Check(changes.changedWater >= kMinShadedFraction * changes.waterPixels, "the tagged water pixels are reshaded");

    for (int view = 1; view <= 5; ++view)
    {
        Config debug = on;
        debug.waterDebugView = view;
        vf_test_set_config(&debug);
        const WaterFrameResult result = client.Render(pass);
        SaveImage(outDir, (L"water-debug-" + std::to_wstring(view)).c_str(), result.image);
    }
    vf_test_set_config(&on);

    CheckSkipsAndUntouchedFrames(h, client, base, stock.image);
    Check(AssignWaterData(MakeSyntheticWaterData()), "synthetic water data re-assigned");
    CheckDepthWriteNesting(h, client);
    CheckOpticsAgainstReference(client, base, outDir);
    CheckWaterDepthReachesFog(h, base);
    CheckStencilClearedEachPass(client, base);
    CheckUntaggedLiquidKeepsItsColour(client, base);
    CheckClassesShadeSeparately(client, base, outDir);
    CheckFlatFallback(client, base, stock.image, outDir);
    CheckReflectionsOffSelectTheSkyOnlyVariant(client, base);
    CheckStockFogOnWater(h, base);
    CheckStockFoggedCopyIsFoggedOnce(h, base);
    CheckInteriorIgnoresSun(h, base);
    CheckWaterFollowsTheDirectLight(h, base);
    CheckGrazingReflectionsMissTheShore(h, base, outDir);
    CheckFoamTintsAreLinear(client, base);
    CheckSunsetFoamStaysBelowLitGround(h, base);
    CheckEndReportsWhatItShaded(h, client, base);
    CheckWaterGpuTimeSummary(client, base);
    CheckFaultInsideThePassRestoresTheDevice(h, client, base);
    CheckWaterStatusListsTheShadedClasses(client, base);
    CheckWaterOffReleasesResources(client, base);
    CheckFaultedFirstBeginReleasesItsResources(client, base);
    CheckFailedFoamMaskUploadsRetry(client, base);
    CheckFlatWaterHasNoCrestFoam(client, base);
    CheckShoreFoamWidthFollowsTheSlope(h, base);
    CheckOccluderEdgesKeepWaveDetail(h, base);
    CheckReflectionsCarryTheirSourcesFog(h, base);
    CheckSkyReflectionsCarryTheSkysFog(h, base);
    CheckCrestFoamAccumulates(client, base, waterMask.image);
    CheckResetKeepsWater(h, client, base);
    CheckRealDataViews(h, base, waterDataPath, outDir);

    vf_test_set_water_seconds(kRealTime);
    vf_test_set_config(&base);
}
}
