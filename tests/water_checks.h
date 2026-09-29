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
constexpr uint32_t kOtherSunColour = 0xFF4060FF;
constexpr Vec3 kGrazingEye = {0, 0, 4};
constexpr Vec3 kGrazingTarget = {100, 5, 1};
constexpr int kReflectionDebugView = 4;
constexpr UINT kMirroredShoreRows = 8;
constexpr int kShoreColourTolerance = 3;
constexpr float kMaxShoreReflectionFraction = 0.02f;
constexpr float kRoughTileAmplitude = 30000.0f;
constexpr float kRoughTileWindMultiplier = 3.0f;
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
};

const WaterStrip kLakeBasin[] = {{-kBasinHalfWidth, kBasinHalfWidth, kLakeClass, kStockWaterColour}};
const WaterStrip kClassStrips[] = {
    {90.0f, kBasinHalfWidth, kLakeClass, kStockWaterColour},
    {-80.0f, 90.0f, kOceanClass, kStockWaterColour},
    {-kBasinHalfWidth, -80.0f, kRiverClass, kStockWaterColour},
};
const WaterStrip kHiddenLake[] = {{-kBasinHalfWidth, kBasinHalfWidth, kLakeClass, kInvisibleWaterColour}};
const WaterStrip kInteriorBasin[] = {{-kBasinHalfWidth, kBasinHalfWidth, kInteriorClass, kStockWaterColour}};

enum class WaterCalls
{
    None,
    Pass,
    PassWithoutTags,
};

struct WaterFrame
{
    const WaterStrip* strips = kLakeBasin;
    int stripCount = 1;
    WaterCalls calls = WaterCalls::Pass;
    const WaterStrip* hiddenTaggedStrips = nullptr;
    int hiddenTaggedStripCount = 0;
    bool opaqueMask = false;
    bool fogDepthView = false;
};

struct WaterFrameResult
{
    Image image;
    bool began = false;
    const char* skip = "";
    bool stateKept = true;
    bool tagWritesDocumented = true;
    bool untagRestores = true;
};

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
        if (frame.calls != WaterCalls::None)
            result.began = vf_test_water_begin(&m_v.in, &m_v.water, &result.skip) != 0;
        for (int i = 0; i < frame.stripCount; ++i)
            DrawStrip(frame.strips[i], frame.calls == WaterCalls::Pass, frame.opaqueMask, result);
        for (int i = 0; i < frame.hiddenTaggedStripCount; ++i)
            DrawStrip(frame.hiddenTaggedStrips[i], true, false, result);
        m_h.DrawPretransformedQuadAtRawDepth(kOtherPassLeft, kOtherPassTop, kOtherPassRight, kOtherPassBottom,
                                             kOtherPassRawDepth, kOtherPassColour);
        ApplyClientDrawState();
        if (frame.calls != WaterCalls::None)
            vf_test_water_end();
        WaterSentinel after;
        ReadWaterSentinel(dev, after);
        result.stateKept = SameWaterSentinel(before, after, true);
        ReleaseWaterSentinel(before);
        ReleaseWaterSentinel(after);
        if (frame.fogDepthView)
        {
            const char* skip = "";
            vf_test_render(&m_v.in, &skip);
        }
        result.image = Capture(dev);
        dev->EndScene();
        dev->Present(nullptr, nullptr, nullptr, nullptr);
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
        dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
        dev->SetTexture(0, nullptr);
        SelectDiffuseColour();
        dev->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
        dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, static_cast<UINT>(m_scene.size() / 3), m_scene.data(),
                             sizeof(SceneVertex));
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

    void DrawStrip(const WaterStrip& strip, bool tag, bool opaqueMask, WaterFrameResult& result)
    {
        IDirect3DDevice9* dev = m_h.dev;
        WaterSentinel beforeTag;
        ReadWaterSentinel(dev, beforeTag);
        if (tag)
        {
            vf_test_water_tag(strip.waterClass);
            WaterSentinel tagged;
            ReadWaterSentinel(dev, tagged);
            const bool documented = result.began ? OnlyDocumentedStencilWrites(beforeTag, tagged, strip.waterClass)
                                                 : SameWaterSentinel(beforeTag, tagged, true);
            result.tagWritesDocumented = result.tagWritesDocumented && documented;
            ReleaseWaterSentinel(tagged);
        }
        const DWORD colour = opaqueMask ? kWaterMaskColour : strip.colour;
        SceneVertex quad[6];
        const Vec3 corners[4] = {{kBasinNearX, strip.y0, kWaterSurfaceZ}, {kBasinFarX, strip.y0, kWaterSurfaceZ},
                                 {kBasinFarX, strip.y1, kWaterSurfaceZ}, {kBasinNearX, strip.y1, kWaterSurfaceZ}};
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
        if (tag)
        {
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
    vf_test_water_end();
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
    vf_test_water_end();
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

void CheckInteriorIgnoresSun(Harness& h, const Config& base)
{
    const Config on = WaterConfig(base);
    vf_test_set_config(&on);
    WaterView sunny = DefaultWaterView();
    WaterView otherSun = sunny;
    otherSun.in.sunColor = kOtherSunColour;
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

void SaveRealDataViews(Harness& h, const Config& base, const std::string& waterDataPath, const std::wstring& outDir)
{
    const DWORD attributes = GetFileAttributesA(waterDataPath.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY))
    {
        std::printf("     %s not found; real water data views skipped\n", waterDataPath.c_str());
        return;
    }
    if (!vf_test_load_water_data(waterDataPath.c_str()))
    {
        std::printf("     %s did not load; real water data views skipped\n", waterDataPath.c_str());
        return;
    }
    const Config cfg = WaterConfig(base);
    vf_test_set_config(&cfg);
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
        BasinClient client(h, MakeWaterView(view.eye, view.at));
        WaterFrame frame;
        frame.strips = view.strips;
        frame.stripCount = view.strips == kClassStrips ? 3 : 1;
        const WaterFrameResult result = client.Render(frame);
        std::printf("     %ls: pass %s%s\n", view.name, result.began ? "drawn" : "skipped: ", result.skip);
        SaveImage(outDir, view.name, result.image);
    }
    std::printf("     the real water data stays loaded after these views\n");
}

void CheckWaterPass(Harness& h, const std::wstring& outDir, const std::string& waterDataPath)
{
    Config base;
    vf_test_get_config(&base);
    vf_test_set_water_seconds(kFrameSeconds);
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
    Check(shaded.tagWritesDocumented, "tagging a water draw changes only the documented stencil states");
    Check(shaded.untagRestores, "untagging restores the client's stencil states");
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
    CheckClassesShadeSeparately(client, base, outDir);
    CheckFlatFallback(client, base, stock.image, outDir);
    CheckStockFogOnWater(h, base);
    CheckInteriorIgnoresSun(h, base);
    CheckGrazingReflectionsMissTheShore(h, base, outDir);
    CheckResetKeepsWater(h, client, base);
    SaveRealDataViews(h, base, waterDataPath, outDir);

    vf_test_set_water_seconds(kRealTime);
    vf_test_set_config(&base);
}
}
