#include "config.h"
#include "engine.h"
#include "fog_data.h"
#include "fog_model.h"

#include <windows.h>
#include <d3d9.h>
#include <wincodec.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

extern "C" __declspec(dllimport) IDirect3D9* __cdecl vf_test_wrap_direct3d9(IDirect3D9*(WINAPI*)(UINT), UINT);
extern "C" __declspec(dllimport) void __cdecl vf_test_set_config(const Config*);
extern "C" __declspec(dllimport) void __cdecl vf_test_get_config(Config*);
extern "C" __declspec(dllimport) int __cdecl vf_test_render(const FrameInputs*, const char**);
extern "C" __declspec(dllimport) void __cdecl vf_test_force_depth_write(int);
extern "C" __declspec(dllimport) int __cdecl vf_test_overlay_visible();
extern "C" __declspec(dllimport) void __cdecl vf_test_draw_overlay();

namespace
{
constexpr D3DFORMAT kIntz = static_cast<D3DFORMAT>(MAKEFOURCC('I', 'N', 'T', 'Z'));
constexpr float kPi = 3.14159265f;
constexpr float kNear = 0.4f;
constexpr float kFar = 1000.0f;
constexpr float kFovY = 0.9f;
constexpr float kIdealWorldMaxZ = 1.0f;
constexpr float kClientWorldMaxZ = 0.94f;
constexpr float kClientWdlRawDepth = 0.9985f;
constexpr float kNoWdlPatch = 0.0f;
constexpr float kDisplayGamma = 2.2f;
constexpr int kReferenceIntegrationSteps = 8192;
constexpr BYTE kKeyHeld = 0x80;
constexpr UINT kKeyRepeatCountOne = 1;
constexpr UINT kKeyReleaseTransition = 0xC0000000u;
constexpr int kOverlaySettleFrames = 3;
constexpr UINT kOverlayProbeLeft = 40;
constexpr UINT kOverlayProbeTop = 60;
constexpr UINT kOverlayProbeRight = 400;
constexpr UINT kOverlayProbeBottom = 300;
constexpr int kOverlayTitleBarX = 200;
constexpr int kOverlayTitleBarY = 40;
constexpr int kBesideOverlayX = 1100;
constexpr int kBesideOverlayY = 600;
constexpr int kOverlayBodyX = 480;
constexpr int kOverlayBodyY = 75;
constexpr int kDensitySliderY = 217;
constexpr int kDensitySliderGrabX = 60;
constexpr int kDensitySliderDragX = 250;
constexpr float kDensityAfterDragAtLeast = 2.5f;

int g_failures = 0;

void Check(bool ok, const char* what)
{
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        ++g_failures;
}

struct Vec3
{
    float x, y, z;
};

Vec3 Add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 Sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 Cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 Norm(Vec3 a)
{
    float l = std::sqrt(Dot(a, a));
    return {a.x / l, a.y / l, a.z / l};
}

constexpr Vec3 kWorldUp = {0, 0, 1};
constexpr Vec3 kGameLikeWorldOffset = {-9100.0f, -100.0f, 80.0f};

float EyeHeightAboveGround(Vec3 eye)
{
    return eye.z - kGameLikeWorldOffset.z;
}

void CameraRelativeLookAt(Vec3 eye, Vec3 at, float* view)
{
    Vec3 forward = Norm(Sub(at, eye));
    Vec3 right = Norm(Cross(forward, kWorldUp));
    Vec3 up = Cross(right, forward);
    float rowVectors[16] = {right.x, up.x, forward.x, 0, right.y, up.y, forward.y, 0,
                            right.z, up.z, forward.z, 0, 0,       0,    0,         1};
    std::memcpy(view, rowVectors, sizeof(rowVectors));
}

void EngineGlDepthProjection(float yScale, float aspect, float zNear, float zFar, float* m)
{
    float p[16] = {yScale / aspect, 0, 0, 0, 0, yScale, 0, 0, 0, 0, (zFar + zNear) / (zFar - zNear), 1,
                   0, 0, -2.0f * zFar * zNear / (zFar - zNear), 0};
    std::memcpy(m, p, sizeof(p));
}

void EngineProjection(float aspect, float* m)
{
    EngineGlDepthProjection(1.0f / std::tan(kFovY * 0.5f), aspect, kNear, kFar, m);
}

void RemapToD3DDepthRange(const float* glDepthProjection, float* d3dProjection)
{
    std::memcpy(d3dProjection, glDepthProjection, sizeof(float) * 16);
    d3dProjection[10] = (1.0f + glDepthProjection[10]) * 0.5f;
    d3dProjection[14] = glDepthProjection[14] * 0.5f;
}

struct SceneVertex
{
    float x, y, z;
    DWORD color;
};

void AddQuad(std::vector<SceneVertex>& v, Vec3 a, Vec3 b, Vec3 c, Vec3 d, DWORD color,
             Vec3 offset = kGameLikeWorldOffset)
{
    for (Vec3 p : {a, b, c, a, c, d})
    {
        Vec3 w = Add(p, offset);
        v.push_back({w.x, w.y, w.z, color});
    }
}

void AddBox(std::vector<SceneVertex>& v, Vec3 lo, Vec3 hi, DWORD color)
{
    Vec3 p[8] = {{lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z}, {hi.x, hi.y, lo.z}, {lo.x, hi.y, lo.z},
                 {lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z}, {hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z}};
    AddQuad(v, p[0], p[1], p[5], p[4], color);
    AddQuad(v, p[1], p[2], p[6], p[5], color);
    AddQuad(v, p[2], p[3], p[7], p[6], color);
    AddQuad(v, p[3], p[0], p[4], p[7], color);
    AddQuad(v, p[4], p[5], p[6], p[7], color);
}

std::vector<SceneVertex> BuildScene()
{
    std::vector<SceneVertex> v;
    AddQuad(v, {-2000, -2000, 0}, {2000, -2000, 0}, {2000, 2000, 0}, {-2000, 2000, 0}, 0xFF4F6B3A);
    for (int i = 0; i < 14; ++i)
    {
        float x = 40.0f + i * 22.0f;
        float y = (i % 2 ? -1.0f : 1.0f) * (8.0f + (i * 7) % 30);
        AddBox(v, {x, y, 0}, {x + 4, y + 4, 25.0f + (i * 13) % 30}, 0xFF6E5A48);
    }
    AddBox(v, {600, -300, 0}, {640, 300, 90}, 0xFF5A5A62);
    return v;
}

bool SavePng(const std::wstring& path, UINT w, UINT h, const std::vector<unsigned char>& bgra)
{
    IWICImagingFactory* factory = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
        return false;
    IWICStream* stream = nullptr;
    IWICBitmapEncoder* encoder = nullptr;
    IWICBitmapFrameEncode* frame = nullptr;
    bool ok = SUCCEEDED(factory->CreateStream(&stream)) &&
              SUCCEEDED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE)) &&
              SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) &&
              SUCCEEDED(encoder->Initialize(stream, WICBitmapEncoderNoCache)) &&
              SUCCEEDED(encoder->CreateNewFrame(&frame, nullptr)) && SUCCEEDED(frame->Initialize(nullptr)) &&
              SUCCEEDED(frame->SetSize(w, h));
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    ok = ok && SUCCEEDED(frame->SetPixelFormat(&format)) &&
         SUCCEEDED(frame->WritePixels(h, w * 4, static_cast<UINT>(bgra.size()), const_cast<BYTE*>(bgra.data()))) &&
         SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit());
    if (frame)
        frame->Release();
    if (encoder)
        encoder->Release();
    if (stream)
        stream->Release();
    factory->Release();
    return ok;
}

struct Image
{
    UINT w = 0, h = 0;
    std::vector<unsigned char> bgra;

    const unsigned char* At(UINT x, UINT y) const { return &bgra[(static_cast<size_t>(y) * w + x) * 4]; }
    float Luma(UINT x, UINT y) const
    {
        const unsigned char* p = At(x, y);
        return (0.114f * p[0] + 0.587f * p[1] + 0.299f * p[2]) / 255.0f;
    }
};

Image Capture(IDirect3DDevice9* dev)
{
    Image img;
    IDirect3DSurface9* bb = nullptr;
    IDirect3DSurface9* sys = nullptr;
    dev->GetRenderTarget(0, &bb);
    D3DSURFACE_DESC desc;
    bb->GetDesc(&desc);
    dev->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &sys, nullptr);
    dev->GetRenderTargetData(bb, sys);
    D3DLOCKED_RECT lr;
    sys->LockRect(&lr, nullptr, D3DLOCK_READONLY);
    img.w = desc.Width;
    img.h = desc.Height;
    img.bgra.resize(static_cast<size_t>(img.w) * img.h * 4);
    for (UINT y = 0; y < img.h; ++y)
    {
        std::memcpy(&img.bgra[static_cast<size_t>(y) * img.w * 4], static_cast<unsigned char*>(lr.pBits) + y * lr.Pitch,
                    img.w * 4);
        for (UINT x = 0; x < img.w; ++x)
            img.bgra[(static_cast<size_t>(y) * img.w + x) * 4 + 3] = 255;
    }
    sys->UnlockRect();
    sys->Release();
    bb->Release();
    return img;
}

constexpr DWORD kFogPassTextureStages = 4;
constexpr UINT kFogPassPixelConstants = 36;

struct Sentinel
{
    DWORD renderStates[10];
    DWORD samplers[kFogPassTextureStages][4];
    IDirect3DBaseTexture9* textures[kFogPassTextureStages];
    IDirect3DVertexShader9* vs;
    IDirect3DPixelShader9* ps;
    IDirect3DVertexDeclaration9* decl;
    IDirect3DVertexBuffer9* stream;
    UINT streamOffset, streamStride;
    float constants[kFogPassPixelConstants * 4];
    D3DVIEWPORT9 viewport;
    RECT scissor;
    IDirect3DSurface9* rt;
    IDirect3DSurface9* ds;
};

const D3DRENDERSTATETYPE kSentinelStates[10] = {
    D3DRS_ZENABLE,  D3DRS_ZWRITEENABLE,      D3DRS_ALPHABLENDENABLE, D3DRS_SRCBLEND,  D3DRS_DESTBLEND,
    D3DRS_CULLMODE, D3DRS_SCISSORTESTENABLE, D3DRS_COLORWRITEENABLE, D3DRS_FOGENABLE, D3DRS_STENCILENABLE,
};
const D3DSAMPLERSTATETYPE kSentinelSamplers[4] = {D3DSAMP_ADDRESSU, D3DSAMP_MAGFILTER, D3DSAMP_MINFILTER,
                                                  D3DSAMP_MIPFILTER};

void ReadSentinel(IDirect3DDevice9* dev, Sentinel& s)
{
    std::memset(&s, 0, sizeof(s));
    for (int i = 0; i < 10; ++i)
        dev->GetRenderState(kSentinelStates[i], &s.renderStates[i]);
    for (DWORD t = 0; t < kFogPassTextureStages; ++t)
    {
        for (int i = 0; i < 4; ++i)
            dev->GetSamplerState(t, kSentinelSamplers[i], &s.samplers[t][i]);
        dev->GetTexture(t, &s.textures[t]);
    }
    dev->GetVertexShader(&s.vs);
    dev->GetPixelShader(&s.ps);
    dev->GetVertexDeclaration(&s.decl);
    dev->GetStreamSource(0, &s.stream, &s.streamOffset, &s.streamStride);
    dev->GetPixelShaderConstantF(0, s.constants, kFogPassPixelConstants);
    dev->GetViewport(&s.viewport);
    dev->GetScissorRect(&s.scissor);
    dev->GetRenderTarget(0, &s.rt);
    dev->GetDepthStencilSurface(&s.ds);
}

void ReleaseSentinel(Sentinel& s)
{
    IUnknown* refs[] = {s.textures[0], s.textures[1], s.textures[2], s.textures[3], s.vs,
                        s.ps,          s.decl,        s.stream,      s.rt,          s.ds};
    for (IUnknown* r : refs)
        if (r)
            r->Release();
}

void ReportSentinelDifferences(const Sentinel& a, const Sentinel& b)
{
    for (int i = 0; i < 10; ++i)
        if (a.renderStates[i] != b.renderStates[i])
            std::printf("     render state %d: %lu -> %lu\n", kSentinelStates[i], a.renderStates[i], b.renderStates[i]);
    for (DWORD t = 0; t < kFogPassTextureStages; ++t)
    {
        for (int i = 0; i < 4; ++i)
            if (a.samplers[t][i] != b.samplers[t][i])
                std::printf("     sampler %d state %d: %lu -> %lu\n", t, kSentinelSamplers[i], a.samplers[t][i],
                            b.samplers[t][i]);
        if (a.textures[t] != b.textures[t])
            std::printf("     texture %d: %p -> %p\n", t, static_cast<void*>(a.textures[t]),
                        static_cast<void*>(b.textures[t]));
    }
    if (a.vs != b.vs || a.ps != b.ps || a.decl != b.decl)
        std::printf("     shaders/decl: vs %p->%p ps %p->%p decl %p->%p\n", static_cast<void*>(a.vs),
                    static_cast<void*>(b.vs), static_cast<void*>(a.ps), static_cast<void*>(b.ps),
                    static_cast<void*>(a.decl), static_cast<void*>(b.decl));
    if (a.stream != b.stream || a.streamOffset != b.streamOffset || a.streamStride != b.streamStride)
        std::printf("     stream 0: %p+%u/%u -> %p+%u/%u\n", static_cast<void*>(a.stream), a.streamOffset,
                    a.streamStride, static_cast<void*>(b.stream), b.streamOffset, b.streamStride);
    for (UINT i = 0; i < kFogPassPixelConstants * 4; ++i)
        if (a.constants[i] != b.constants[i])
        {
            std::printf("     pixel constant c%d differs\n", i / 4);
            break;
        }
    if (std::memcmp(&a.viewport, &b.viewport, sizeof(a.viewport)) != 0)
        std::printf("     viewport %lu,%lu %lux%lu -> %lu,%lu %lux%lu\n", a.viewport.X, a.viewport.Y, a.viewport.Width,
                    a.viewport.Height, b.viewport.X, b.viewport.Y, b.viewport.Width, b.viewport.Height);
    if (std::memcmp(&a.scissor, &b.scissor, sizeof(a.scissor)) != 0)
        std::printf("     scissor differs\n");
    if (a.rt != b.rt || a.ds != b.ds)
        std::printf("     targets: rt %p->%p ds %p->%p\n", static_cast<void*>(a.rt), static_cast<void*>(b.rt),
                    static_cast<void*>(a.ds), static_cast<void*>(b.ds));
}

bool SameSentinel(const Sentinel& a, const Sentinel& b)
{
    return std::memcmp(a.renderStates, b.renderStates, sizeof(a.renderStates)) == 0 &&
           std::memcmp(a.samplers, b.samplers, sizeof(a.samplers)) == 0 &&
           std::memcmp(a.textures, b.textures, sizeof(a.textures)) == 0 && a.vs == b.vs && a.ps == b.ps &&
           a.decl == b.decl && a.stream == b.stream && a.streamOffset == b.streamOffset &&
           a.streamStride == b.streamStride && std::memcmp(a.constants, b.constants, sizeof(a.constants)) == 0 &&
           std::memcmp(&a.viewport, &b.viewport, sizeof(a.viewport)) == 0 &&
           std::memcmp(&a.scissor, &b.scissor, sizeof(a.scissor)) == 0 && a.rt == b.rt && a.ds == b.ds;
}

struct Harness
{
    HWND window = nullptr;
    IDirect3D9* d3d = nullptr;
    IDirect3DDevice9* dev = nullptr;
    D3DPRESENT_PARAMETERS pp = {};
    std::vector<SceneVertex> scene = BuildScene();
    IDirect3DTexture9* dummyTexture = nullptr;
    IDirect3DVertexBuffer9* dummyBuffer = nullptr;
    IDirect3DVertexShader9* engineVs = nullptr;
    IDirect3DPixelShader9* enginePs = nullptr;
    IDirect3DVertexDeclaration9* engineDecl = nullptr;
    D3DCOLOR clearColor = 0xFF6FA0DC;

    void CreateEngineObjects()
    {
        dev->CreateTexture(4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &dummyTexture, nullptr);
        dev->CreateVertexBuffer(256, D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &dummyBuffer, nullptr);
        static const D3DVERTEXELEMENT9 elements[] = {
            {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
            D3DDECL_END(),
        };
        dev->CreateVertexDeclaration(elements, &engineDecl);
    }

    void ReleaseEngineObjects()
    {
        if (dummyTexture)
            dummyTexture->Release();
        if (dummyBuffer)
            dummyBuffer->Release();
        if (engineDecl)
            engineDecl->Release();
        dummyTexture = nullptr;
        dummyBuffer = nullptr;
        engineDecl = nullptr;
    }

    void BeginFrame()
    {
        dev->BeginScene();
        dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
        dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        for (DWORD t = 0; t < kFogPassTextureStages; ++t)
            dev->SetTexture(t, nullptr);
        dev->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, clearColor, 1.0f, 0);
    }

    void DrawScene(Vec3 eye, const float* view, const float* engineProj, const D3DVIEWPORT9& vp)
    {
        float d3dProj[16];
        RemapToD3DDepthRange(engineProj, d3dProj);
        D3DMATRIX cameraRelativeWorld = {};
        cameraRelativeWorld._11 = cameraRelativeWorld._22 = cameraRelativeWorld._33 = cameraRelativeWorld._44 = 1.0f;
        cameraRelativeWorld._41 = -eye.x;
        cameraRelativeWorld._42 = -eye.y;
        cameraRelativeWorld._43 = -eye.z;
        dev->SetTransform(D3DTS_WORLD, &cameraRelativeWorld);
        dev->SetTransform(D3DTS_VIEW, reinterpret_cast<const D3DMATRIX*>(view));
        dev->SetTransform(D3DTS_PROJECTION, reinterpret_cast<const D3DMATRIX*>(d3dProj));
        dev->SetViewport(&vp);
        dev->SetVertexShader(nullptr);
        dev->SetPixelShader(nullptr);
        dev->SetRenderState(D3DRS_LIGHTING, FALSE);
        dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        dev->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        dev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
        dev->SetTexture(0, nullptr);
        dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
        dev->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
        dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, static_cast<UINT>(scene.size() / 3), scene.data(),
                             sizeof(SceneVertex));
    }

    void DrawPretransformedQuadAtRawDepth(float x0, float y0, float x1, float y1, float rawDepth)
    {
        struct ScreenVertex
        {
            float x, y, z, rhw;
            DWORD color;
        };
        const ScreenVertex quad[6] = {
            {x0, y0, rawDepth, 1, 0xFF808890}, {x1, y0, rawDepth, 1, 0xFF808890}, {x0, y1, rawDepth, 1, 0xFF808890},
            {x1, y0, rawDepth, 1, 0xFF808890}, {x1, y1, rawDepth, 1, 0xFF808890}, {x0, y1, rawDepth, 1, 0xFF808890},
        };
        D3DVIEWPORT9 vp = {};
        dev->GetViewport(&vp);
        D3DVIEWPORT9 fullDepthRange = vp;
        fullDepthRange.MinZ = 0.0f;
        fullDepthRange.MaxZ = 1.0f;
        dev->SetViewport(&fullDepthRange);
        dev->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
        dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, quad, sizeof(ScreenVertex));
        dev->SetViewport(&vp);
    }

    void DrawDistantTerrainPatch(float wdlRawDepth)
    {
        DrawPretransformedQuadAtRawDepth(0.0f, 0.0f, 200.0f, 100.0f, wdlRawDepth);
    }

    void SetEngineState(const D3DVIEWPORT9& vp)
    {
        dev->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        dev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_DESTCOLOR);
        dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
        dev->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
        dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0x7);
        dev->SetRenderState(D3DRS_FOGENABLE, TRUE);
        dev->SetRenderState(D3DRS_STENCILENABLE, TRUE);
        for (DWORD t = 0; t < kFogPassTextureStages; ++t)
        {
            dev->SetTexture(t, dummyTexture);
            dev->SetSamplerState(t, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR);
            dev->SetSamplerState(t, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC);
            dev->SetSamplerState(t, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
            dev->SetSamplerState(t, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
        }
        float constants[kFogPassPixelConstants * 4];
        for (UINT i = 0; i < kFogPassPixelConstants * 4; ++i)
            constants[i] = 0.25f * i - 3.0f;
        dev->SetPixelShaderConstantF(0, constants, kFogPassPixelConstants);
        dev->SetVertexDeclaration(engineDecl);
        dev->SetStreamSource(0, dummyBuffer, 16, 16);
        dev->SetViewport(&vp);
        RECT scissor = {3, 5, 700, 400};
        dev->SetScissorRect(&scissor);
    }
};

FrameInputs MakeInputs(const float* view, const float* proj, Vec3 eye, Vec3 at, const D3DVIEWPORT9& vp)
{
    FrameInputs in = {};
    std::memcpy(in.cameraRelativeView, view, sizeof(in.cameraRelativeView));
    std::memcpy(in.glProjection, proj, sizeof(in.glProjection));
    in.camPos[0] = eye.x;
    in.camPos[1] = eye.y;
    in.camPos[2] = eye.z;
    Vec3 f = Norm(Sub(at, eye));
    in.camTarget[0] = eye.x + f.x * 6;
    in.camTarget[1] = eye.y + f.y * 6;
    in.camTarget[2] = eye.z - 6;
    in.viewport = vp;
    in.dayFraction = 0.75f;
    Vec3 sun = Norm({1.0f, 0.12f, 0.16f});
    in.toLight[0] = sun.x;
    in.toLight[1] = sun.y;
    in.toLight[2] = sun.z;
    in.lightIsMoon = false;
    in.fogColor = 0xFF9DB2C8;
    in.sunColor = 0xFFFFD9A0;
    in.directColor = 0xFFE8D8C0;
    in.ambientColor = 0xFF404858;
    in.fogStart = 150.0f;
    in.fogEnd = 600.0f;
    in.zoneFogDistance = 600.0f;
    in.farClip = kFar;
    in.inLiquid = false;
    in.mapId = -1;
    return in;
}

float SmoothStep(float e0, float e1, float x)
{
    float t = std::fmin(std::fmax((x - e0) / (e1 - e0), 0.0f), 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float ReferenceUnshadowedSkyTransmittance(const FrameInputs& in, const Config& cfg, const AuthoredFog* authored,
                                          float px, float py, int steps, float jitter)
{
    FogParams fog = BuildFogParams(in, cfg, authored);
    const float* P = in.glProjection;
    const D3DVIEWPORT9& vp = in.viewport;
    float ndcX = (px - vp.X) / vp.Width * 2.0f - 1.0f;
    float ndcY = 1.0f - (py - vp.Y) / vp.Height * 2.0f;
    Vec3 ray = {(ndcX - P[8]) / P[0], (ndcY - P[9]) / P[5], 1.0f};
    float len = std::sqrt(Dot(ray, ray));
    Vec3 v = {ray.x / len, ray.y / len, ray.z / len};
    const float* m = in.cameraRelativeView;
    Vec3 dirW = {v.x * m[0] + v.y * m[1] + v.z * m[2], v.x * m[4] + v.y * m[5] + v.z * m[6],
                 v.x * m[8] + v.y * m[9] + v.z * m[10]};
    float z = fog.maxDistance;
    z = z + (fog.maxDistance - z) * SmoothStep(fog.horizonStart, fog.farClip, z);
    float tMax = std::fmin(z * len, fog.maxDistance);
    double tau = 0.0;
    for (int s = 0; s < steps; ++s)
    {
        float u0 = static_cast<float>(s) / steps;
        float u1 = u0 + 1.0f / steps;
        float ta = tMax * u0 * u0;
        float tb = tMax * u1 * u1;
        float dt = tb - ta;
        float t = ta + (tb - ta) * jitter;
        float h = in.camPos[2] + dirW.z * t;
        auto clamp01 = [](float x) { return std::fmin(std::fmax(x, 0.0f), 1.0f); };
        for (const FogLayer& l : fog.layers)
        {
            float scale = std::exp(-std::fmax(dirW.z, 0.0f) * l.skyFalloff);
            float cover =
                clamp01((t - l.start) / std::fmax(dt, 1e-3f)) * clamp01((l.endDistance - ta) / std::fmax(dt, 1e-3f));
            float curve = 1.0f + l.strength * std::pow(std::fmin(std::fmax(t - l.start, 0.0f) / fog.maxDistance, 1.0f) +
                                                           1e-6f,
                                                       l.exponent);
            float heightF = std::fmin(std::exp((l.upperHeight - h) * l.upperFalloff), 1.0f) *
                            std::fmin(std::exp((h - l.lowerHeight) * l.lowerFalloff), 1.0f);
            tau += l.density * scale * dt * cover * curve * heightF;
        }
    }
    return static_cast<float>(std::exp(-tau));
}

const AuthoredLayer* FarWall(const AuthoredFog& fog)
{
    for (int i = 0; i < fog.layerCount; ++i)
        if (fog.layers[i].start >= 1000.0f)
            return &fog.layers[i];
    return nullptr;
}

const AuthoredLayer* MidHaze(const AuthoredFog& fog)
{
    for (int i = 0; i < fog.layerCount; ++i)
        if (fog.layers[i].start < 1.0f && std::fabs(fog.layers[i].g - 0.5f) < 0.01f)
            return &fog.layers[i];
    return nullptr;
}

float WeightOf(const AuthoredFog& fog, uint32_t light)
{
    for (int i = 0; i < fog.lightCount; ++i)
        if (fog.lightIds[i] == light)
            return fog.lightWeights[i];
    return 0.0f;
}

constexpr int kEasternKingdoms = 0;
constexpr int kKalimdor = 1;
constexpr int kOutland = 530;
const LightParamsSelection kClearWeather = {};
constexpr uint32_t kEasternKingdomsGlobalLight = 1;
constexpr uint32_t kStormwindLight = 77;
constexpr float kDayFraction1800 = 0.75f;
constexpr float kDayFraction1845 = 2250.0f / 2880.0f;

void CheckClassicData(const FogData& data)
{
    const float harbourAtStormwindLightEdge[3] = {-8565.21f, 993.46f, 104.96f};
    AuthoredFog fog = {};
    bool ok = data.Resolve(kEasternKingdoms, harbourAtStormwindLightEdge, kDayFraction1800, kClearWeather, fog);
    float globalWeight = WeightOf(fog, kEasternKingdomsGlobalLight);
    float stormwindWeight = WeightOf(fog, kStormwindLight);
    const AuthoredLayer* wall = FarWall(fog);
    const AuthoredLayer* haze = MidHaze(fog);
    std::printf("     harbour at 18:00: lights 1:%.3f 77:%.3f, %d layers, far wall density %.4f, haze density %.4f\n",
                globalWeight, stormwindWeight, fog.layerCount, wall ? wall->density : -1.0f,
                haze ? haze->density : -1.0f);
    Check(ok && fog.layerCount == 3 && std::fabs(globalWeight + stormwindWeight - 1.0f) < 1e-4f &&
              stormwindWeight > 0.05f && stormwindWeight < 0.1f,
          "Classic light blend at the harbour (light 77 falloff edge)");
    Check(wall && haze && std::fabs(wall->start - 3000.0f) < 0.5f &&
              std::fabs(wall->density - (globalWeight * 0.6f + stormwindWeight * 0.1f)) < 1e-4f &&
              std::fabs(haze->density - (globalWeight * 0.12f + stormwindWeight * 0.1f)) < 1e-4f,
          "Classic layers blended by light weight at a key time");

    AuthoredFog mid = {};
    data.Resolve(kEasternKingdoms, harbourAtStormwindLightEdge, kDayFraction1845, kClearWeather, mid);
    float expected = WeightOf(mid, kEasternKingdomsGlobalLight) * 0.8f + WeightOf(mid, kStormwindLight) * 0.3f;
    const AuthoredLayer* midWall = FarWall(mid);
    std::printf("     harbour at 18:45: far wall density %.4f (expected %.4f)\n", midWall ? midWall->density : -1.0f,
                expected);
    Check(midWall && std::fabs(midWall->density - expected) < 1e-4f,
          "Classic keys interpolated between 18:00 and 19:30");

    AuthoredFog none = {};
    Check(!data.Resolve(kOutland, harbourAtStormwindLightEdge, kDayFraction1800, kClearWeather, none),
          "maps without Classic lights fall back to derived layers");
}

constexpr float kNoon = 0.5f;
constexpr uint32_t kElwynnZoneLight = 16849;
constexpr uint32_t kOrgrimmarDragZoneLight = 16845;
constexpr uint32_t kOrgrimmarZoneLight = 239;
constexpr int kUnusedLightParamsSlot = 6;
const float kOpenSeaWestOfElwynn[3] = {-10500.0f, 2500.0f, 50.0f};

bool Near(float a, float b)
{
    return std::fabs(a - b) < 1e-4f;
}

LightParamsSelection Storm(float blend)
{
    LightParamsSelection selection;
    selection.stormBlend = blend;
    return selection;
}

LightParamsSelection ScreenEffectSlot(int slot, float stormBlend)
{
    LightParamsSelection selection = Storm(stormBlend);
    selection.screenEffectSlot = slot;
    return selection;
}

void CheckStormBlendsLayersByClassicIndex(const FogData& data)
{
    AuthoredFog clear = {};
    AuthoredFog storm = {};
    AuthoredFog half = {};
    bool resolved = data.Resolve(kEasternKingdoms, kOpenSeaWestOfElwynn, kNoon, kClearWeather, clear) &&
                    data.Resolve(kEasternKingdoms, kOpenSeaWestOfElwynn, kNoon, Storm(1.0f), storm) &&
                    data.Resolve(kEasternKingdoms, kOpenSeaWestOfElwynn, kNoon, Storm(0.5f), half);
    std::printf("     open sea at noon, densities by Classic layer index: clear %.3f %.3f %.3f, storm %.3f %.3f %.3f, "
                "half storm %.3f %.3f %.3f (far wall start %.0f)\n",
                clear.layers[0].density, clear.layers[1].density, clear.layers[2].density, storm.layers[0].density,
                storm.layers[1].density, storm.layers[2].density, half.layers[0].density, half.layers[1].density,
                half.layers[2].density, half.layers[1].start);
    Check(resolved && clear.lightCount == 1 && clear.lightIds[0] == kEasternKingdomsGlobalLight &&
              Near(clear.layers[1].start, 3000.0f) && Near(clear.layers[1].density, 0.3f) &&
              Near(clear.layers[2].density, 0.1f),
          "open sea resolves only the Eastern Kingdoms light");
    Check(storm.layerCount == 3 && storm.layers[0].density == 0.0f && Near(storm.layers[1].start, 800.0f) &&
              Near(storm.layers[1].density, 0.75f) && Near(storm.layers[2].density, 0.75f),
          "a full storm uses the storm layers at their Classic indices");
    Check(Near(half.layers[0].density, 0.015f) && Near(half.layers[0].g, clear.layers[0].g) &&
              Near(half.layers[1].start, 1900.0f) && Near(half.layers[1].density, 0.525f) &&
              Near(half.layers[2].density, 0.425f),
          "a half storm blends matching layers and thins the clear-only haze");
}

void CheckScreenEffectLightSlot(const FogData& data)
{
    AuthoredFog ghost = {};
    AuthoredFog unused = {};
    AuthoredFog clear = {};
    const LightParamsSelection ghostDuringStorm = ScreenEffectSlot(FogData::kDeathSlot, 1.0f);
    const LightParamsSelection unusedSlot = ScreenEffectSlot(kUnusedLightParamsSlot, 0.0f);
    bool resolved = data.Resolve(kEasternKingdoms, kOpenSeaWestOfElwynn, kNoon, ghostDuringStorm, ghost) &&
                    data.Resolve(kEasternKingdoms, kOpenSeaWestOfElwynn, kNoon, unusedSlot, unused) &&
                    data.Resolve(kEasternKingdoms, kOpenSeaWestOfElwynn, kNoon, kClearWeather, clear);
    std::printf("     ghost at noon: densities %.3f %.3f %.3f, start %.0f\n", ghost.layers[0].density,
                ghost.layers[1].density, ghost.layers[2].density, ghost.layers[2].start);
    Check(resolved && ghost.layerCount == 3 && ghost.layers[0].density == 0.0f && ghost.layers[1].density == 0.0f &&
              Near(ghost.layers[2].start, 75.0f) && Near(ghost.layers[2].density, 1.0f),
          "the ghost screen effect selects the death slot over the storm");
    Check(Near(unused.layers[1].density, clear.layers[1].density) &&
              Near(unused.layers[2].density, clear.layers[2].density),
          "a screen effect slot the light leaves empty keeps its weather layers");
}

void CheckZoneLights(const FogData& data)
{
    const float elwynnInterior[3] = {-9847.0f, 49.0f, 50.0f};
    const float elwynnFiftyYardsInsideWestEdge[3] = {-9847.0f, 664.0f, 50.0f};
    const float orgrimmarDragInsideOrgrimmar[3] = {1847.0f, -4499.0f, 20.0f};
    AuthoredFog inside = {};
    AuthoredFog edge = {};
    AuthoredFog drag = {};
    bool resolved = data.Resolve(kEasternKingdoms, elwynnInterior, kNoon, kClearWeather, inside) &&
                    data.Resolve(kEasternKingdoms, elwynnFiftyYardsInsideWestEdge, kNoon, kClearWeather, edge) &&
                    data.Resolve(kKalimdor, orgrimmarDragInsideOrgrimmar, kNoon, kClearWeather, drag);
    const float edgeZoneWeight = WeightOf(edge, kElwynnZoneLight);
    std::printf("     Elwynn zone light: interior %.2f (far wall %.2f), 50 yd inside the edge %.2f; Drag %.2f, "
                "Orgrimmar %.2f\n",
                WeightOf(inside, kElwynnZoneLight), inside.layers[1].density, edgeZoneWeight,
                WeightOf(drag, kOrgrimmarDragZoneLight), WeightOf(drag, kOrgrimmarZoneLight));
    Check(resolved && Near(WeightOf(inside, kElwynnZoneLight), 1.0f) &&
              WeightOf(inside, kEasternKingdomsGlobalLight) == 0.0f && Near(inside.layers[1].density, 0.2f),
          "inside its outline the Elwynn zone light replaces the Eastern Kingdoms light");
    Check(edgeZoneWeight > 0.4f && edgeZoneWeight < 0.6f &&
              Near(edgeZoneWeight + WeightOf(edge, kEasternKingdomsGlobalLight), 1.0f),
          "a zone light fades in over 100 yd inside its outline");
    Check(Near(WeightOf(drag, kOrgrimmarDragZoneLight), 1.0f) && WeightOf(drag, kOrgrimmarZoneLight) == 0.0f,
          "the innermost of nested zone outlines wins");
}

float SunlitLevelRayOpacity(const FogParams& fog, float camZ, float distance)
{
    const float dt = distance / kReferenceIntegrationSteps;
    double tau = 0.0;
    for (int i = 0; i < kReferenceIntegrationSteps; ++i)
    {
        const float t = (i + 0.5f) * dt;
        for (const FogLayer& l : fog.layers)
        {
            if (t < l.start || t > l.endDistance)
                continue;
            float curve =
                1.0f + l.strength * std::pow(std::fmin(std::fmax(t - l.start, 0.0f) / fog.maxDistance, 1.0f) + 1e-6f,
                                             l.exponent);
            float heightF = std::fmin(std::exp((l.upperHeight - camZ) * l.upperFalloff), 1.0f) *
                            std::fmin(std::exp((camZ - l.lowerHeight) * l.lowerFalloff), 1.0f);
            float shadow =
                l.shadowed > 0.0f ? l.shadowDensity + (1.0f - l.shadowDensity) * fog.shadowedLayerLightScale : 1.0f;
            tau += l.density * curve * heightF * shadow * dt;
        }
    }
    return static_cast<float>(1.0 - std::exp(-tau));
}

constexpr float kContinentFogStart = 197.9f;
constexpr float kContinentFarClip = 791.6f;
constexpr Vec3 kHarbourEye = {-8576.0f, 1007.0f, 104.0f};
constexpr float kHarbourDayFraction = 0.7802f;
constexpr Vec3 kHarbourToLight = {0.673f, 0.673f, 0.307f};

FrameInputs ContinentFrame(int map, Vec3 eye, float dayFraction, Vec3 toLight, bool lightIsMoon)
{
    const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    const D3DVIEWPORT9 vp = {0, 0, 1280, 720, 0.0f, kClientWorldMaxZ};
    FrameInputs in = MakeInputs(identity, identity, eye, Add(eye, {100, 0, 0}), vp);
    in.mapId = map;
    in.dayFraction = dayFraction;
    Vec3 l = Norm(toLight);
    in.toLight[0] = l.x;
    in.toLight[1] = l.y;
    in.toLight[2] = l.z;
    in.lightIsMoon = lightIsMoon;
    in.fogStart = kContinentFogStart;
    in.fogEnd = kContinentFarClip;
    in.zoneFogDistance = kContinentFarClip;
    in.farClip = kContinentFarClip;
    return in;
}

void CheckStormFogFollowsClientDirectLight(const FogData& data)
{
    const Config cfg = {};
    const Vec3 goldshire = {-9456.8f, 54.7f, 59.6f};
    const float eightPm = 0.8337f;
    const Vec3 lowSun = {0.704f, 0.704f, 0.086f};
    constexpr uint32_t kClientStormDirectLight = 0xFF656565;
    constexpr uint32_t kWhiteDirectLight = 0xFFFFFFFF;
    FrameInputs storm = ContinentFrame(kEasternKingdoms, goldshire, eightPm, lowSun, false);
    storm.directColor = kClientStormDirectLight;
    storm.lightParams = Storm(1.0f);
    FrameInputs bright = storm;
    bright.directColor = kWhiteDirectLight;
    AuthoredFog fog = {};
    bool resolved = data.Resolve(kEasternKingdoms, storm.camPos, eightPm, storm.lightParams, fog);
    FogParams dim = BuildFogParams(storm, cfg, &fog);
    FogParams full = BuildFogParams(bright, cfg, &fog);
    const float diffuseRatio = dim.layers[2].diffuse[0] / std::fmax(full.layers[2].diffuse[0], 1e-6f);
    std::printf("     Goldshire storm 20:00: client direct light vs Classic %.2f (white light %.2f), "
                "sun scatter %.2f -> %.2f\n",
                dim.directLightMatch, full.directLightMatch, full.layers[2].diffuse[0], dim.layers[2].diffuse[0]);
    Check(resolved && dim.directLightMatch > 0.35f && dim.directLightMatch < 0.47f &&
              Near(diffuseRatio, dim.directLightMatch),
          "storm fog scatters the client's dimmer storm sunlight, not Classic's");
    Check(full.directLightMatch == 1.0f, "a client direct light brighter than Classic's never brightens the fog");

    const Vec3 darkshire = {-10559.2f, -1196.6f, 28.3f};
    const float sixPm = 0.7511f;
    const Vec3 eveningSun = {0.641f, 0.641f, 0.423f};
    constexpr uint32_t kClientDuskwoodDirectLight = 0xFF344A5C;
    FrameInputs clearDuskwood = ContinentFrame(kEasternKingdoms, darkshire, sixPm, eveningSun, false);
    clearDuskwood.directColor = kClientDuskwoodDirectLight;
    AuthoredFog duskwoodFog = {};
    bool duskwoodResolved =
        data.Resolve(kEasternKingdoms, clearDuskwood.camPos, sixPm, clearDuskwood.lightParams, duskwoodFog);
    FogParams duskwood = BuildFogParams(clearDuskwood, cfg, &duskwoodFog);
    std::printf("     Darkshire clear 18:00: direct light match %.2f\n", duskwood.directLightMatch);
    Check(duskwoodResolved && duskwood.directLightMatch == 1.0f,
          "clear weather keeps Classic's authored sun scattering where the client's light is darker");
}

void CheckDenseClassicFogAtHarbourSunset(const FogData& data)
{
    const Config cfg = {};
    FrameInputs harbour = ContinentFrame(kEasternKingdoms, kHarbourEye, kHarbourDayFraction, kHarbourToLight, false);
    AuthoredFog fog = {};
    bool resolved = data.Resolve(kEasternKingdoms, harbour.camPos, harbour.dayFraction, kClearWeather, fog);
    FogParams p = BuildFogParams(harbour, cfg, resolved ? &fog : nullptr);
    float horizon = SunlitLevelRayOpacity(p, harbour.camPos[2], p.maxDistance);
    std::printf("     harbour 18:43: coverage %.2f, distance fog density %.6f, level-ray opacity %.3f\n", fog.coverage,
                p.layers[kDistanceFogLayer].density, horizon);
    Check(resolved && p.layers[kDistanceFogLayer].density == 0.0f && horizon > 0.97f,
          "dense Classic layers hide the far clip without the distance fog");
}

void CheckThinClassicFogAtHyjalMidnight(const FogData& data)
{
    const Config cfg = {};
    const Vec3 hyjal = {5458.0f, -2934.0f, 1481.0f};
    const float midnight = 0.0f;
    const Vec3 toMoon = {0.63f, 0.63f, 0.455f};
    FrameInputs frame = ContinentFrame(kKalimdor, hyjal, midnight, toMoon, true);
    AuthoredFog fog = {};
    bool resolved = data.Resolve(kKalimdor, frame.camPos, frame.dayFraction, kClearWeather, fog);
    FogParams p = BuildFogParams(frame, cfg, resolved ? &fog : nullptr);
    float horizon = SunlitLevelRayOpacity(p, frame.camPos[2], p.maxDistance);
    std::printf("     Hyjal 00:00: coverage %.2f, distance fog density %.6f, level-ray opacity %.3f\n", fog.coverage,
                p.layers[kDistanceFogLayer].density, horizon);
    Check(resolved && p.layers[kDistanceFogLayer].density > 0.0f && horizon > 0.94f,
          "thin Classic layers keep enough distance fog to hide the far clip");
}

constexpr uint32_t kFoglessBlastedLandsLight = 19;

float SceneLayersDensity(const FogParams& p)
{
    float density = 0.0f;
    for (int i = 0; i < kSceneLayers; ++i)
        density += p.layers[i].density;
    return density;
}

void CheckFogThinsIntoFoglessClassicLight(const FogData& data)
{
    const Config cfg = {};
    const Vec3 foglessBlastedLandsLight19 = {-12041.6f, -2450.3f, 0.0f};
    const float afterNoon = 0.5212f;
    const Vec3 toSun = {0.106f, 0.106f, 0.989f};
    const float offsets[] = {0.0f, -455.0f, -465.0f};
    FrameInputs frames[3];
    AuthoredFog fogs[3] = {};
    FogParams params[3];
    bool resolved = true;
    for (int i = 0; i < 3; ++i)
    {
        frames[i] = ContinentFrame(kEasternKingdoms, Add(foglessBlastedLandsLight19, {0, offsets[i], 20.0f}),
                                   afterNoon, toSun, false);
        resolved = data.Resolve(kEasternKingdoms, frames[i].camPos, afterNoon, kClearWeather, fogs[i]) && resolved;
        params[i] = BuildFogParams(frames[i], cfg, &fogs[i]);
    }
    const float innerOpacity = SunlitLevelRayOpacity(params[1], frames[1].camPos[2], 450.0f);
    const float outerOpacity = SunlitLevelRayOpacity(params[2], frames[2].camPos[2], 450.0f);
    std::printf("     light 19 at noon: centre scene density %.6f, distance fog %.6f; light 19 weight %.2f / %.2f, "
                "opacity at 450 yd %.3f / %.3f\n",
                SceneLayersDensity(params[0]), params[0].layers[kDistanceFogLayer].density,
                WeightOf(fogs[1], kFoglessBlastedLandsLight), WeightOf(fogs[2], kFoglessBlastedLandsLight),
                innerOpacity, outerOpacity);
    Check(resolved && std::fabs(innerOpacity - outerOpacity) < 0.08f,
          "fog thins continuously into a Classic light without fog");
    Check(Near(WeightOf(fogs[0], kFoglessBlastedLandsLight), 1.0f) && SceneLayersDensity(params[0]) == 0.0f &&
              params[0].layers[kDistanceFogLayer].density > 0.0f,
          "inside a Classic light without fog only the distance fog hides the far clip");
}

float BoxMean7x7(const Image& img, UINT cx, UINT cy, int ch)
{
    double sum = 0.0;
    for (UINT y = cy - 3; y <= cy + 3; ++y)
        for (UINT x = cx - 3; x <= cx + 3; ++x)
            sum += img.At(x, y)[ch];
    return static_cast<float>(sum / 49.0 / 255.0);
}

float HighlightRollOff(float x, float knee)
{
    float span = std::fmax(1.0f - knee, 1e-4f);
    return x <= knee ? x : knee + span * (1.0f - std::exp(-(x - knee) / span));
}

float ExpectedLinearComposite(float sceneEncoded, float radianceEncoded, float transmittance)
{
    float scene = std::pow(sceneEncoded, kDisplayGamma);
    float radiance = std::pow(radianceEncoded, kDisplayGamma);
    return std::pow(HighlightRollOff(scene * transmittance + radiance, std::fmax(0.8f, scene)), 1.0f / kDisplayGamma);
}

float PreInvertClientGlow(float composited, float glow)
{
    return (std::sqrt(1.0f + 4.0f * glow * composited) - 1.0f) / (2.0f * glow);
}

void CheckLinearComposite(Harness& h, const Config& cfg, Vec3 eye, Vec3 at, const float* proj,
                          const D3DVIEWPORT9& world)
{
    constexpr int kConvergenceFrames = 16;
    const char* skip = "";
    float view[16];
    CameraRelativeLookAt(eye, at, view);
    auto renderConvergedWithGlow = [&](const Config& c, float glow) {
        vf_test_set_config(&c);
        Image img;
        for (int frame = 0; frame < kConvergenceFrames; ++frame)
        {
            h.BeginFrame();
            h.DrawScene(eye, view, proj, world);
            FrameInputs in = MakeInputs(view, proj, eye, at, world);
            in.clientGlowAmount = glow;
            vf_test_render(&in, &skip);
            if (frame == kConvergenceFrames - 1)
                img = Capture(h.dev);
            h.dev->EndScene();
        }
        return img;
    };
    auto renderConverged = [&](const Config& c) { return renderConvergedWithGlow(c, 0.0f); };
    h.BeginFrame();
    h.DrawScene(eye, view, proj, world);
    Image scene = Capture(h.dev);
    h.dev->EndScene();

    Config c = cfg;
    c.godRays = 0.0f;
    c.temporal = 0.85f;
    Config none = c;
    none.density = 0.0f;
    Image unfogged = renderConverged(none);
    int maxDiff = 0;
    for (UINT y = 0; y < 688; ++y)
        for (UINT x = 0; x < 1280; ++x)
            for (int ch = 0; ch < 3; ++ch)
                maxDiff = std::max(maxDiff, std::abs(unfogged.At(x, y)[ch] - scene.At(x, y)[ch]));
    std::printf("     zero fog: largest channel change %d/255\n", maxDiff);
    Check(maxDiff <= 1, "zero fog leaves the world viewport unchanged (linear composite)");

    Config radianceCfg = c;
    radianceCfg.debugView = 1;
    Image radiance = renderConverged(radianceCfg);
    Config transmittanceCfg = c;
    transmittanceCfg.debugView = 2;
    Image transmittance = renderConverged(transmittanceCfg);
    Image composited = renderConverged(c);
    const UINT points[3][2] = {{101, 101}, {640, 330}, {640, 600}};
    float worst = 0.0f;
    for (const auto& p : points)
        for (int ch = 0; ch < 3; ++ch)
        {
            float expected = ExpectedLinearComposite(BoxMean7x7(scene, p[0], p[1], ch),
                                                     BoxMean7x7(radiance, p[0], p[1], ch),
                                                     BoxMean7x7(transmittance, p[0], p[1], 2));
            worst = std::fmax(worst, std::fabs(BoxMean7x7(composited, p[0], p[1], ch) - expected));
        }
    std::printf("     linear composite vs scene * T + L: largest difference %.1f/255\n", worst * 255.0f);
    Check(worst < 4.0f / 255.0f, "fog blends over the scene in linear light");

    const float clientGlow = 0.65f;
    Image compensated = renderConvergedWithGlow(c, clientGlow);
    float glowWorst = 0.0f;
    for (const auto& p : points)
        for (int ch = 0; ch < 3; ++ch)
        {
            float composite = BoxMean7x7(composited, p[0], p[1], ch);
            float fogOpacity = 1.0f - BoxMean7x7(transmittance, p[0], p[1], 2);
            float expected = composite + (PreInvertClientGlow(composite, clientGlow) - composite) * fogOpacity;
            glowWorst = std::fmax(glowWorst, std::fabs(BoxMean7x7(compensated, p[0], p[1], ch) - expected));
        }
    Config noGlow = c;
    noGlow.glowCompensation = false;
    Image uncompensated = renderConvergedWithGlow(noGlow, clientGlow);
    float offDiff = 0.0f;
    for (const auto& p : points)
        for (int ch = 0; ch < 3; ++ch)
            offDiff = std::fmax(offDiff, std::fabs(BoxMean7x7(uncompensated, p[0], p[1], ch) -
                                                   BoxMean7x7(composited, p[0], p[1], ch)));
    std::printf("     glow compensation: largest difference %.1f/255; switched off %.1f/255\n", glowWorst * 255.0f,
                offDiff * 255.0f);
    Check(glowWorst < 4.0f / 255.0f && offDiff < 1.0f / 255.0f,
          "fog is pre-compensated for the client's glow, and GlowCompensation 0 turns it off");

    Config gamma = c;
    gamma.colorSpace = 0;
    Image gammaImage = renderConverged(gamma);
    double gammaDiff = 0.0;
    for (UINT y = 0; y < 688; ++y)
        for (UINT x = 0; x < 1280; ++x)
            gammaDiff += std::fabs(gammaImage.Luma(x, y) - scene.Luma(x, y));
    gammaDiff /= 1280.0 * 688.0;
    std::printf("     gamma mode: mean luma change %.4f\n", gammaDiff);
    Check(gammaDiff > 0.01 && gammaDiff < 0.5, "gamma mode (ColorSpace 0) still blends the fog");
    vf_test_set_config(&cfg);
}

struct ExposureVariant
{
    float exposure;
    float sunScatter;
};

void SaveClassicSunJustAboveViewCaptures(Harness& h, const Config& cfg, Vec3 eye, const FrameInputs& in,
                                         const float* proj, const D3DVIEWPORT9& world, const std::wstring& outDir)
{
    const char* skip = "";
    Vec3 towardSun = Add(eye, {in.toLight[0] * 100.0f, in.toLight[1] * 100.0f, -2.0f});
    float facingSunView[16];
    CameraRelativeLookAt(eye, towardSun, facingSunView);
    FrameInputs sunJustAboveView = MakeInputs(facingSunView, proj, eye, towardSun, world);
    sunJustAboveView.mapId = kEasternKingdoms;
    sunJustAboveView.toLight[2] = 0.45f;
    float len = std::sqrt(sunJustAboveView.toLight[0] * sunJustAboveView.toLight[0] +
                          sunJustAboveView.toLight[1] * sunJustAboveView.toLight[1] + 0.2025f);
    for (float& v : sunJustAboveView.toLight)
        v /= len;
    const ExposureVariant variants[] = {{1.0f, 1.0f}, {0.6f, 1.0f}, {0.6f, 0.6f}, {0.45f, 0.8f}};
    for (const ExposureVariant& v : variants)
    {
        Config sunCfg = cfg;
        sunCfg.dataMode = 1;
        sunCfg.exposure = v.exposure;
        sunCfg.sunScatter = v.sunScatter;
        vf_test_set_config(&sunCfg);
        for (int frame = 0; frame < 8; ++frame)
        {
            h.BeginFrame();
            h.DrawScene(eye, facingSunView, proj, world);
            vf_test_render(&sunJustAboveView, &skip);
            if (frame == 7)
            {
                Image sunImage = Capture(h.dev);
                wchar_t name[96];
                swprintf(name, 96, L"\\classic-sun-exposure%.2f-sun%.2f.png", v.exposure, v.sunScatter);
                SavePng(outDir + name, sunImage.w, sunImage.h, sunImage.bgra);
            }
            h.dev->EndScene();
        }
    }
}

std::wstring FullPath(const std::wstring& path)
{
    wchar_t full[MAX_PATH] = {};
    return GetFullPathNameW(path.c_str(), MAX_PATH, full, nullptr) ? std::wstring(full) : path;
}

std::string NarrowPath(const std::wstring& path)
{
    char narrow[MAX_PATH] = {};
    WideCharToMultiByte(CP_ACP, 0, path.c_str(), -1, narrow, MAX_PATH, nullptr, nullptr);
    return narrow;
}

std::string ReadText(const std::wstring& path)
{
    std::string text;
    FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f)
        return text;
    char chunk[4096];
    for (size_t n; (n = std::fread(chunk, 1, sizeof(chunk), f)) > 0;)
        text.append(chunk, n);
    std::fclose(f);
    return text;
}

bool SameHotkey(const Hotkey& a, const Hotkey& b)
{
    return a.virtualKey == b.virtualKey && a.ctrl == b.ctrl && a.shift == b.shift && a.alt == b.alt;
}

void CheckOverlayKeyNames()
{
    Hotkey key = {};
    Check(ParseHotkey("Ctrl+F7", key) && SameHotkey(key, {VK_F7, true, false, false}), "OverlayKey Ctrl+F7 parses");
    Check(ParseHotkey(" shift + ALT + pageup ", key) && SameHotkey(key, {VK_PRIOR, false, true, true}),
          "OverlayKey modifiers and key names ignore case and spaces");
    Check(ParseHotkey("o", key) && SameHotkey(key, {'O', false, false, false}) && ParseHotkey("F24", key) &&
              key.virtualKey == VK_F24,
          "OverlayKey takes letters and F1-F24");
    Hotkey untouched = kDefaultOverlayHotkey;
    const char* const malformed[] = {"", "Ctrl+", "F25", "F0", "Ctrl+F7+F8", "Win+F7", "Space"};
    bool rejected = true;
    for (const char* text : malformed)
        rejected = !ParseHotkey(text, untouched) && rejected;
    Check(rejected && SameHotkey(untouched, kDefaultOverlayHotkey), "malformed OverlayKey values are rejected");
    Check(HotkeyName(kDefaultOverlayHotkey) == "Ctrl+F7" &&
              HotkeyName({VK_HOME, false, true, true}) == "Shift+Alt+Home",
          "overlay key names read back as written");
}

void CheckSettingsSaveKeepsTheIni(const std::wstring& outDir, const std::wstring& shippedIni)
{
    const std::wstring savedIni = FullPath(outDir + L"\\saved.ini");
    Check(CopyFileW(shippedIni.c_str(), savedIni.c_str(), FALSE) != FALSE, "shipped CoAVolFog.ini copied");
    ConfigStore store;
    store.Load(NarrowPath(savedIni));
    const Config shipped = store.Get();
    Check(!store.HasUnsavedChanges(), "a freshly loaded INI has no unsaved changes");

    Config edited = shipped;
    edited.enable = false;
    edited.overlay = false;
    edited.quality = 3;
    edited.density = 2.5f;
    edited.temporal = 0.5f;
    edited.lightShafts = !shipped.lightShafts;
    edited.farClipMax = 900.0f;
    edited.debugView = 2;
    edited.godRays = 99.0f;
    store.Apply(edited);
    const Config applied = store.Get();
    Check(applied.enable == shipped.enable && applied.overlay == shipped.overlay && applied.godRays == 4.0f &&
              store.HasUnsavedChanges(),
          "overlay edits keep the restart-only switches and are clamped like the INI");
    Check(store.Save() && !store.HasUnsavedChanges(), "overlay settings save to the INI");

    ConfigStore reloaded;
    reloaded.Load(NarrowPath(savedIni));
    const Config& r = reloaded.Get();
    Check(SameLiveSettings(r, applied) && r.quality == 3 && r.density == 2.5f && r.farClipMax == 900.0f &&
              r.lightShafts == edited.lightShafts && r.enable == shipped.enable && r.overlay == shipped.overlay,
          "saved settings reload unchanged and the restart-only keys stay as they were");
    const std::string text = ReadText(savedIni);
    Check(text.rfind("; CoAVolFog settings.", 0) == 0 && text.find("\nDensity=2.5") != std::string::npos &&
              text.find("\nHaze=1.0") != std::string::npos && text.find("\nEnable=1") != std::string::npos &&
              text.find("; Global density multiplier") != std::string::npos,
          "saving rewrites only the changed lines and keeps the comments");

    Config scratch = store.Get();
    scratch.density = 0.1f;
    store.Apply(scratch);
    store.Revert();
    Check(store.Get().density == 2.5f && !store.HasUnsavedChanges(), "Revert goes back to the saved settings");

    Config typed = store.Get();
    typed.haze = 1.23456f;
    store.Apply(typed);
    WritePrivateProfileStringW(L"CoAVolFog", L"Quality", L"1", savedIni.c_str());
    const bool mergedSave = store.Save();
    ConfigStore afterMerge;
    afterMerge.Load(NarrowPath(savedIni));
    Check(mergedSave && store.Get().quality == 1 && afterMerge.Get().quality == 1 && afterMerge.Get().density == 2.5f,
          "Save keeps a hand edit of a setting the window did not change");
    Check(store.Get().haze == afterMerge.Get().haze && !store.HasUnsavedChanges() &&
              ReadText(savedIni).find("\nHaze=1.235") != std::string::npos,
          "Save records the value it wrote, rounded like the INI");
}

struct ClientInput
{
    int keyDowns = 0;
    int mouseDowns = 0;
    int mouseUps = 0;
    int mouseMoves = 0;
};

ClientInput g_clientInput;

LRESULT CALLBACK ClientWindowProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN)
        ++g_clientInput.keyDowns;
    if (msg == WM_LBUTTONDOWN)
        ++g_clientInput.mouseDowns;
    if (msg == WM_LBUTTONUP)
        ++g_clientInput.mouseUps;
    if (msg == WM_MOUSEMOVE)
        ++g_clientInput.mouseMoves;
    return DefWindowProcW(window, msg, wParam, lParam);
}

void HoldModifiers(bool ctrl, bool shift, bool alt)
{
    BYTE keys[256] = {};
    GetKeyboardState(keys);
    keys[VK_CONTROL] = ctrl ? kKeyHeld : 0;
    keys[VK_SHIFT] = shift ? kKeyHeld : 0;
    keys[VK_MENU] = alt ? kKeyHeld : 0;
    SetKeyboardState(keys);
}

void PressKey(HWND window, unsigned virtualKey)
{
    const UINT pressed = kKeyRepeatCountOne | (MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC) << 16);
    SendMessageW(window, WM_KEYDOWN, virtualKey, static_cast<LPARAM>(pressed));
    SendMessageW(window, WM_KEYUP, virtualKey, static_cast<LPARAM>(pressed | kKeyReleaseTransition));
}

void PressHotkey(HWND window, const Hotkey& key)
{
    HoldModifiers(key.ctrl, key.shift, key.alt);
    PressKey(window, key.virtualKey);
    HoldModifiers(false, false, false);
}

LPARAM ClientPointOfBackBufferPixel(const Harness& h, int x, int y)
{
    RECT client = {};
    GetClientRect(h.window, &client);
    return MAKELPARAM(MulDiv(x, client.right, static_cast<int>(h.pp.BackBufferWidth)),
                      MulDiv(y, client.bottom, static_cast<int>(h.pp.BackBufferHeight)));
}

void DrawOverlayFrames(int frames)
{
    for (int i = 0; i < frames; ++i)
        vf_test_draw_overlay();
}

void ClickAfterHover(HWND window, LPARAM point)
{
    SendMessageW(window, WM_MOUSEMOVE, 0, point);
    DrawOverlayFrames(1);
    SendMessageW(window, WM_LBUTTONDOWN, MK_LBUTTON, point);
    SendMessageW(window, WM_LBUTTONUP, 0, point);
    DrawOverlayFrames(2);
}

double MeanLumaChange(const Image& a, const Image& b, UINT x0, UINT y0, UINT x1, UINT y1)
{
    double sum = 0.0;
    for (UINT y = y0; y < y1; ++y)
        for (UINT x = x0; x < x1; ++x)
            sum += std::fabs(a.Luma(x, y) - b.Luma(x, y));
    return sum / (static_cast<double>(x1 - x0) * (y1 - y0));
}

double OverlayProbeChange(const Image& a, const Image& b)
{
    return MeanLumaChange(a, b, kOverlayProbeLeft, kOverlayProbeTop, kOverlayProbeRight, kOverlayProbeBottom);
}

void CheckOverlayInput(Harness& h)
{
    const Hotkey key = kDefaultOverlayHotkey;
    g_clientInput = {};
    PressHotkey(h.window, key);
    Check(vf_test_overlay_visible() == 1 && g_clientInput.keyDowns == 0,
          "the overlay key opens the overlay without reaching the client");
    PressKey(h.window, 'W');
    Check(g_clientInput.keyDowns == 1, "other keys reach the client while the overlay is open");

    DrawOverlayFrames(kOverlaySettleFrames);
    ClickAfterHover(h.window, ClientPointOfBackBufferPixel(h, kOverlayTitleBarX, kOverlayTitleBarY));
    Check(g_clientInput.mouseDowns == 0, "clicks on the overlay window do not reach the client");
    Check(g_clientInput.mouseMoves == 1, "mouse moves over the overlay window still reach the client");
    ClickAfterHover(h.window, ClientPointOfBackBufferPixel(h, kBesideOverlayX, kBesideOverlayY));
    Check(g_clientInput.mouseDowns == 1, "clicks beside the overlay window reach the client");

    PressHotkey(h.window, key);
    Check(vf_test_overlay_visible() == 0 && g_clientInput.keyDowns == 1, "the overlay key closes the overlay");

    Config off = {};
    off.overlay = false;
    vf_test_set_config(&off);
    PressHotkey(h.window, key);
    Check(vf_test_overlay_visible() == 0 && g_clientInput.keyDowns == 2,
          "with Overlay=0 the overlay key reaches the client");
    Config on = {};
    vf_test_set_config(&on);
}

void DragAcross(HWND window, LPARAM from, LPARAM to)
{
    SendMessageW(window, WM_MOUSEMOVE, 0, from);
    DrawOverlayFrames(1);
    SendMessageW(window, WM_LBUTTONDOWN, MK_LBUTTON, from);
    DrawOverlayFrames(2);
    SendMessageW(window, WM_MOUSEMOVE, MK_LBUTTON, to);
    DrawOverlayFrames(2);
    SendMessageW(window, WM_LBUTTONUP, 0, to);
    DrawOverlayFrames(2);
}

void CheckOverlayWidgets(Harness& h)
{
    Config start = {};
    vf_test_set_config(&start);
    PressHotkey(h.window, kDefaultOverlayHotkey);
    DrawOverlayFrames(kOverlaySettleFrames);
    DragAcross(h.window, ClientPointOfBackBufferPixel(h, kDensitySliderGrabX, kDensitySliderY),
               ClientPointOfBackBufferPixel(h, kDensitySliderDragX, kDensitySliderY));
    Config dragged = {};
    vf_test_get_config(&dragged);
    std::printf("     Density after dragging its slider: %.2f\n", dragged.density);
    Check(dragged.density > kDensityAfterDragAtLeast, "the Density slider drags although its section is named Density");

    g_clientInput = {};
    const LPARAM panelBody = ClientPointOfBackBufferPixel(h, kOverlayBodyX, kOverlayBodyY);
    ClickAfterHover(h.window, panelBody);
    DrawOverlayFrames(kOverlaySettleFrames);
    PressKey(h.window, VK_TAB);
    DrawOverlayFrames(kOverlaySettleFrames);
    PressKey(h.window, 'W');
    std::printf("     key presses the client saw for Tab then W: %d\n", g_clientInput.keyDowns);
    Check(g_clientInput.keyDowns == 2, "Tab with the overlay focused does not take the keyboard from the client");

    g_clientInput = {};
    const LPARAM beside = ClientPointOfBackBufferPixel(h, kBesideOverlayX, kBesideOverlayY);
    SendMessageW(h.window, WM_MOUSEMOVE, 0, beside);
    DrawOverlayFrames(kOverlaySettleFrames);
    SendMessageW(h.window, WM_MOUSEMOVE, 0, panelBody);
    SendMessageW(h.window, WM_LBUTTONDOWN, MK_LBUTTON, panelBody);
    DrawOverlayFrames(2);
    SendMessageW(h.window, WM_LBUTTONUP, 0, panelBody);
    std::printf("     button presses / releases the client saw: %d / %d\n", g_clientInput.mouseDowns,
                g_clientInput.mouseUps);
    Check(g_clientInput.mouseDowns == 1 && g_clientInput.mouseUps == 1,
          "a button whose press reached the client is released to the client");
    PressHotkey(h.window, kDefaultOverlayHotkey);
    vf_test_set_config(&start);

    Config pauseKey = {};
    pauseKey.overlayKey = {VK_PAUSE, true, false, false};
    vf_test_set_config(&pauseKey);
    HoldModifiers(true, false, false);
    PressKey(h.window, VK_CANCEL);
    const bool opened = vf_test_overlay_visible() == 1;
    PressKey(h.window, VK_CANCEL);
    HoldModifiers(false, false, false);
    Check(opened && vf_test_overlay_visible() == 0,
          "Ctrl+Pause toggles the overlay although Windows reports it as Cancel");
    vf_test_set_config(&start);
}

void DrawEngineFrameWithoutPresent(Harness& h, const D3DVIEWPORT9& world)
{
    h.BeginFrame();
    h.SetEngineState(world);
    h.dev->EndScene();
}

void CheckOverlayDraw(Harness& h, const D3DVIEWPORT9& world, const std::wstring& outDir)
{
    IDirect3DIndexBuffer9* engineIndices = nullptr;
    h.dev->CreateIndexBuffer(64, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_MANAGED, &engineIndices, nullptr);
    PressHotkey(h.window, kDefaultOverlayHotkey);
    Image before;
    Image after;
    bool statesKept = true;
    bool indicesKept = engineIndices != nullptr;
    for (int frame = 0; frame < kOverlaySettleFrames; ++frame)
    {
        DrawEngineFrameWithoutPresent(h, world);
        h.dev->SetIndices(engineIndices);
        before = Capture(h.dev);
        Sentinel s0;
        ReadSentinel(h.dev, s0);
        vf_test_draw_overlay();
        Sentinel s1;
        ReadSentinel(h.dev, s1);
        if (!SameSentinel(s0, s1))
        {
            if (statesKept)
                ReportSentinelDifferences(s0, s1);
            statesKept = false;
        }
        ReleaseSentinel(s0);
        ReleaseSentinel(s1);
        IDirect3DIndexBuffer9* boundIndices = nullptr;
        h.dev->GetIndices(&boundIndices);
        indicesKept = boundIndices == engineIndices && indicesKept;
        if (boundIndices)
            boundIndices->Release();
        after = Capture(h.dev);
    }
    h.dev->SetIndices(nullptr);
    if (engineIndices)
        engineIndices->Release();
    SavePng(outDir + L"\\overlay.png", after.w, after.h, after.bgra);
    Check(statesKept && indicesKept,
          "the overlay restores render, sampler, shader, constant, stream, index, viewport, scissor and target state");
    const double panelChange = OverlayProbeChange(before, after);
    std::printf("     mean luma change under the overlay window: %.4f\n", panelChange);
    Check(panelChange > 0.05, "the overlay draws its window over the frame");
    Check(MeanLumaChange(before, after, kBesideOverlayX - 200, kBesideOverlayY - 100, after.w, after.h) == 0.0,
          "the overlay leaves the rest of the frame untouched");

    PressHotkey(h.window, kDefaultOverlayHotkey);
    DrawEngineFrameWithoutPresent(h, world);
    const Image hiddenBefore = Capture(h.dev);
    vf_test_draw_overlay();
    Check(MeanLumaChange(hiddenBefore, Capture(h.dev), 0, 0, hiddenBefore.w, hiddenBefore.h) == 0.0,
          "the hidden overlay draws nothing");
    PressHotkey(h.window, kDefaultOverlayHotkey);
    DrawOverlayFrames(1);
}

int Run(const std::wstring& outDir, const std::string& dataPath, const std::wstring& iniPath)
{
    FogData classic;
    Check(classic.Load(dataPath), "Classic fog data loads");
    CheckClassicData(classic);
    CheckStormBlendsLayersByClassicIndex(classic);
    CheckScreenEffectLightSlot(classic);
    CheckZoneLights(classic);
    CheckStormFogFollowsClientDirectLight(classic);
    CheckDenseClassicFogAtHarbourSunset(classic);
    CheckThinClassicFogAtHyjalMidnight(classic);
    CheckFogThinsIntoFoglessClassicLight(classic);

    CreateDirectoryW(outDir.c_str(), nullptr);
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    CheckOverlayKeyNames();
    CheckSettingsSaveKeepsTheIni(outDir, FullPath(iniPath));

    WNDCLASSW wc = {};
    wc.lpfnWndProc = ClientWindowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"vfog_harness";
    RegisterClassW(&wc);
    Harness h;
    h.window = CreateWindowW(L"vfog_harness", L"vfog", WS_OVERLAPPEDWINDOW, 0, 0, 1280, 720, nullptr, nullptr,
                             wc.hInstance, nullptr);

    HMODULE d3d9 = LoadLibraryA("d3d9.dll");
    auto realCreate = reinterpret_cast<IDirect3D9*(WINAPI*)(UINT)>(GetProcAddress(d3d9, "Direct3DCreate9"));
    h.d3d = vf_test_wrap_direct3d9(realCreate, D3D_SDK_VERSION);
    Check(h.d3d != nullptr, "wrapped Direct3DCreate9");
    if (!h.d3d)
        return 1;

    D3DMULTISAMPLE_TYPE ms = D3DMULTISAMPLE_4_SAMPLES;
    Check(h.d3d->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, TRUE, ms, nullptr) ==
              D3DERR_NOTAVAILABLE,
          "multisampling reported unavailable while fog is enabled");

    h.pp.Windowed = TRUE;
    h.pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    h.pp.BackBufferWidth = 1280;
    h.pp.BackBufferHeight = 720;
    h.pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    h.pp.EnableAutoDepthStencil = TRUE;
    h.pp.AutoDepthStencilFormat = D3DFMT_D24S8;
    h.pp.hDeviceWindow = h.window;
    h.pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    DWORD engineFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE | D3DCREATE_FPU_PRESERVE;
    Check(engineFlags == 0x52, "harness uses the client's device flags");
    Config startup = {};
    vf_test_set_config(&startup);
    HRESULT hr = h.d3d->CreateDevice(0, D3DDEVTYPE_HAL, h.window, engineFlags, &h.pp, &h.dev);
    Check(SUCCEEDED(hr) && h.dev, "CreateDevice through the wrapper");
    if (!h.dev)
        return 1;

    D3DDEVICE_CREATION_PARAMETERS cp;
    h.dev->GetCreationParameters(&cp);
    Check((cp.BehaviorFlags & D3DCREATE_PUREDEVICE) == 0, "pure-device flag removed");
    Check(h.pp.EnableAutoDepthStencil == TRUE && h.pp.AutoDepthStencilFormat == D3DFMT_D24S8,
          "engine-visible depth parameters preserved");
    IDirect3DSurface9* depth = nullptr;
    h.dev->GetDepthStencilSurface(&depth);
    D3DSURFACE_DESC depthDesc = {};
    if (depth)
        depth->GetDesc(&depthDesc);
    Check(depth && depthDesc.Format == kIntz && depthDesc.Width == 1280, "INTZ depth bound as the device depth");
    if (depth)
        depth->Release();
    IDirect3D9* parent = nullptr;
    h.dev->GetDirect3D(&parent);
    Check(parent == h.d3d, "GetDirect3D returns the wrapper");
    if (parent)
        parent->Release();

    h.CreateEngineObjects();
    const float aspect = 1280.0f / 688.0f;
    const D3DVIEWPORT9 world = {0, 0, 1280, 688, 0.0f, 1.0f};
    float proj[16];
    EngineProjection(aspect, proj);
    Vec3 eye = Add({0, 0, 9}, kGameLikeWorldOffset);
    Vec3 at = Add({100, 2, 4}, kGameLikeWorldOffset);
    float view[16];

    Config cfg = {};
    cfg.maxDistance = 5000.0f;
    Config withGodRays = cfg;
    withGodRays.godRays = 0.2f;
    vf_test_set_config(&withGodRays);

    Image before;
    Image after;
    bool allRendered = true;
    bool statesKept = true;
    const char* skip = "";
    for (int frame = 0; frame < 12; ++frame)
    {
        Vec3 e = {eye.x + frame * 0.6f, eye.y, eye.z};
        CameraRelativeLookAt(e, at, view);
        h.BeginFrame();
        h.DrawScene(e, view, proj, world);
        if (frame == 11)
            before = Capture(h.dev);
        h.SetEngineState(world);
        Sentinel s0;
        ReadSentinel(h.dev, s0);
        FrameInputs in = MakeInputs(view, proj, e, at, world);
        allRendered = vf_test_render(&in, &skip) != 0 && allRendered;
        Sentinel s1;
        ReadSentinel(h.dev, s1);
        if (!SameSentinel(s0, s1))
        {
            if (statesKept)
                ReportSentinelDifferences(s0, s1);
            statesKept = false;
        }
        ReleaseSentinel(s0);
        ReleaseSentinel(s1);
        if (frame == 11)
            after = Capture(h.dev);
        h.dev->EndScene();
        h.dev->Present(nullptr, nullptr, nullptr, nullptr);
    }
    Check(allRendered, (std::string("fog rendered on every frame ") + skip).c_str());
    Check(statesKept, "render, sampler, shader, constant, stream, viewport, scissor and target state restored");

    bool outsideKept = true;
    for (UINT y = 688; y < 720 && outsideKept; ++y)
        for (UINT x = 0; x < 1280; ++x)
            if (std::memcmp(before.At(x, y), after.At(x, y), 3) != 0)
            {
                outsideKept = false;
                break;
            }
    Check(outsideKept, "pixels outside the world viewport untouched");

    double diff = 0;
    for (UINT y = 0; y < 688; ++y)
        for (UINT x = 0; x < 1280; ++x)
            diff += std::fabs(after.Luma(x, y) - before.Luma(x, y));
    diff /= 1280.0 * 688.0;
    std::printf("     mean luma change inside the world viewport: %.4f\n", diff);
    Check(diff > 0.01 && diff < 0.5, "fog visibly changes the world viewport");

    float sunDir[3];
    {
        FrameInputs in = MakeInputs(view, proj, eye, at, world);
        const float* v = view;
        float s[3] = {in.toLight[0] * v[0] + in.toLight[1] * v[4] + in.toLight[2] * v[8],
                      in.toLight[0] * v[1] + in.toLight[1] * v[5] + in.toLight[2] * v[9],
                      in.toLight[0] * v[2] + in.toLight[1] * v[6] + in.toLight[2] * v[10]};
        std::memcpy(sunDir, s, sizeof(s));
    }
    UINT sunX = static_cast<UINT>((sunDir[0] / sunDir[2] * proj[0] * 0.5f + 0.5f) * 1280.0f);
    UINT sunY = static_cast<UINT>((0.5f - sunDir[1] / sunDir[2] * proj[5] * 0.5f) * 688.0f);
    UINT awayX = sunX > 640 ? 60 : 1220;
    float haloGain = after.Luma(sunX, sunY - 40) - before.Luma(sunX, sunY - 40);
    float awayGain = after.Luma(awayX, sunY - 40) - before.Luma(awayX, sunY - 40);
    std::printf("     sky luma change near the sun %.3f, away from the sun %.3f (sun at %u,%u)\n", haloGain, awayGain,
                sunX, sunY);
    Check(haloGain > awayGain, "forward scattering brightens the sky around the sun");

    SavePng(outDir + L"\\before.png", before.w, before.h, before.bgra);
    SavePng(outDir + L"\\after.png", after.w, after.h, after.bgra);

    CheckLinearComposite(h, cfg, eye, at, proj, world);

    auto renderDebugIn = [&](int mode, float maxDist, const D3DVIEWPORT9& vp, float wdlPatchRawDepth) {
        Config c = cfg;
        c.debugView = mode;
        c.maxDistance = maxDist;
        c.temporal = 0.0f;
        vf_test_set_config(&c);
        CameraRelativeLookAt(eye, at, view);
        h.BeginFrame();
        h.DrawScene(eye, view, proj, vp);
        if (wdlPatchRawDepth > kNoWdlPatch)
            h.DrawDistantTerrainPatch(wdlPatchRawDepth);
        FrameInputs in = MakeInputs(view, proj, eye, at, vp);
        in.farClip = std::fmin(in.farClip, maxDist);
        vf_test_render(&in, &skip);
        Image img = Capture(h.dev);
        h.dev->EndScene();
        h.dev->Present(nullptr, nullptr, nullptr, nullptr);
        return img;
    };
    auto renderDebug = [&](int mode, float maxDist) { return renderDebugIn(mode, maxDist, world, kNoWdlPatch); };

    Image radiance = renderDebug(1, 5000.0f);
    SavePng(outDir + L"\\debug-radiance.png", radiance.w, radiance.h, radiance.bgra);
    Image transmittance = renderDebug(2, 5000.0f);
    SavePng(outDir + L"\\debug-transmittance.png", transmittance.w, transmittance.h, transmittance.bgra);

    {
        Config c = cfg;
        c.lightShafts = false;
        vf_test_set_config(&c);
        Config saved = cfg;
        cfg = c;
        Image t = renderDebug(2, 5000.0f);
        cfg = saved;
        CameraRelativeLookAt(eye, at, view);
        FrameInputs in = MakeInputs(view, proj, eye, at, world);
        const UINT samples[3][2] = {{101, 101}, {1181, 101}, {101, 201}};
        bool match = true;
        for (const auto& s : samples)
        {
            float lo = 1.0f;
            float hi = 0.0f;
            for (int j = 0; j < 16; ++j)
            {
                float r = ReferenceUnshadowedSkyTransmittance(in, c, nullptr, s[0] + 0.5f, s[1] + 0.5f, 24,
                                                              (j + 0.5f) / 16.0f);
                lo = std::fmin(lo, r);
                hi = std::fmax(hi, r);
            }
            float got = t.At(s[0], s[1])[2] / 255.0f;
            std::printf("     sky transmittance at %u,%u: shader %.3f, reference %.3f..%.3f\n", s[0], s[1], got, lo,
                        hi);
            match = match && got > lo - 0.02f && got < hi + 0.02f;
        }
        Check(match, "sky transmittance matches the CPU reference (world-space reconstruction)");

        UINT farRow = 0;
        float farZ = 0.0f;
        for (UINT py = 0; py < 688 && !farRow; ++py)
        {
            float ndcY = 1.0f - (py + 0.5f) / 688.0f * 2.0f;
            Vec3 ray = {0.0f, ndcY / proj[5], 1.0f};
            float dz = ray.y * view[9] + ray.z * view[10];
            float z = dz < 0.0f ? -EyeHeightAboveGround(eye) / dz : 1e9f;
            if (z < 0.9f * in.fogEnd)
            {
                farRow = py;
                farZ = z;
            }
        }
        float farT = t.At(640, farRow)[2] / 255.0f;
        std::printf("     ground at %.0f yd (row %u, stock fog end %.0f): transmittance %.3f\n", farZ, farRow,
                    in.fogEnd, farT);
        Check(farRow && farT < 0.15f, "distance fog hides terrain where the stock fog it replaces turns opaque");
        float highSky = t.At(640, 20)[2] / 255.0f;
        std::printf("     sky near the top of the view: transmittance %.3f\n", highSky);
        Check(highSky > 0.5f, "distance fog leaves the upper sky visible");
    }

    {
        Config c = cfg;
        c.temporal = 0.0f;
        c.godRays = 0.0f;
        vf_test_set_config(&c);
        CameraRelativeLookAt(eye, at, view);
        FrameInputs in = MakeInputs(view, proj, eye, at, world);
        h.BeginFrame();
        h.DrawScene(eye, view, proj, world);
        vf_test_render(&in, &skip);
        Image single = Capture(h.dev);
        h.dev->EndScene();
        c.temporal = 0.85f;
        vf_test_set_config(&c);
        Image accumulated;
        for (int frame = 0; frame < 16; ++frame)
        {
            h.BeginFrame();
            h.DrawScene(eye, view, proj, world);
            vf_test_render(&in, &skip);
            if (frame == 15)
                accumulated = Capture(h.dev);
            h.dev->EndScene();
        }
        double diff = 0.0;
        for (UINT y = 0; y < 688; ++y)
            for (UINT x = 0; x < 1280; ++x)
                diff += std::fabs(accumulated.Luma(x, y) - single.Luma(x, y));
        diff /= 1280.0 * 688.0;
        std::printf("     static camera: accumulated vs single-frame mean luma difference %.4f\n", diff);
        Check(diff < 0.01, "temporal accumulation converges on a static camera (reprojection)");
        vf_test_set_config(&cfg);
    }

    {
        Config c = cfg;
        c.lightShafts = false;
        c.godRays = 0.0f;
        c.temporal = 0.0f;
        c.dataMode = 1;
        CameraRelativeLookAt(eye, at, view);
        FrameInputs in = MakeInputs(view, proj, eye, at, world);
        in.mapId = kEasternKingdoms;
        AuthoredFog authored = {};
        bool resolved = classic.Resolve(kEasternKingdoms, in.camPos, in.dayFraction, kClearWeather, authored);
        c.debugView = 2;
        vf_test_set_config(&c);
        h.BeginFrame();
        h.DrawScene(eye, view, proj, world);
        vf_test_render(&in, &skip);
        Image t = Capture(h.dev);
        h.dev->EndScene();
        c.debugView = 0;
        vf_test_set_config(&c);
        h.BeginFrame();
        h.DrawScene(eye, view, proj, world);
        vf_test_render(&in, &skip);
        Image classicImage = Capture(h.dev);
        h.dev->EndScene();
        SavePng(outDir + L"\\after-classic.png", classicImage.w, classicImage.h, classicImage.bgra);
        SaveClassicSunJustAboveViewCaptures(h, cfg, eye, in, proj, world, outDir);

        bool match = resolved;
        const UINT samples[3][2] = {{101, 101}, {1181, 101}, {101, 201}};
        for (const auto& s : samples)
        {
            float lo = 1.0f;
            float hi = 0.0f;
            for (int j = 0; j < 16; ++j)
            {
                float r = ReferenceUnshadowedSkyTransmittance(in, c, resolved ? &authored : nullptr, s[0] + 0.5f,
                                                              s[1] + 0.5f, 24, (j + 0.5f) / 16.0f);
                lo = std::fmin(lo, r);
                hi = std::fmax(hi, r);
            }
            float got = t.At(s[0], s[1])[2] / 255.0f;
            std::printf("     Classic sky transmittance at %u,%u: shader %.3f, reference %.3f..%.3f\n", s[0], s[1], got,
                        lo, hi);
            match = match && got > lo - 0.02f && got < hi + 0.02f;
        }
        Check(match, "Classic-layer sky transmittance matches the CPU reference");
        vf_test_set_config(&cfg);
    }

    Image depthView = renderDebug(3, 255.0f);
    SavePng(outDir + L"\\debug-depth.png", depthView.w, depthView.h, depthView.bgra);
    {
        const UINT centreColumn = 640;
        const UINT groundRowNearBottom = 660;
        float ndcY = 1.0f - (groundRowNearBottom + 0.5f) / 688.0f * 2.0f;
        float ndcX = (centreColumn + 0.5f) / 1280.0f * 2.0f - 1.0f;
        Vec3 ray = {ndcX / proj[0], ndcY / proj[5], 1.0f};
        CameraRelativeLookAt(eye, at, view);
        Vec3 dirW = {ray.x * view[0] + ray.y * view[1] + ray.z * view[2],
                     ray.x * view[4] + ray.y * view[5] + ray.z * view[6],
                     ray.x * view[8] + ray.y * view[9] + ray.z * view[10]};
        float groundViewZ = -EyeHeightAboveGround(eye) / dirW.z;
        float expected = std::fmin(groundViewZ / 255.0f, 1.0f);
        float got = depthView.At(centreColumn, groundRowNearBottom)[2] / 255.0f;
        std::printf("     ground view depth: expected %.2f yd, shader %.2f yd\n", groundViewZ, got * 255.0f);
        Check(std::fabs(got - expected) < 2.5f / 255.0f, "depth linearisation matches the scene geometry");

        D3DVIEWPORT9 clientDepthRange = world;
        clientDepthRange.MaxZ = kClientWorldMaxZ;
        Image clientDepth = renderDebugIn(3, 255.0f, clientDepthRange, kClientWdlRawDepth);
        float clientGot = clientDepth.At(centreColumn, groundRowNearBottom)[2] / 255.0f;
        float wdl = clientDepth.At(100, 50)[2] / 255.0f;
        std::printf("     client depth range: ground %.2f yd (expected %.2f), distant terrain %.2f of max distance\n",
                    clientGot * 255.0f, groundViewZ, wdl);
        Check(std::fabs(clientGot - expected) < 2.5f / 255.0f,
              "depth linearisation follows the world viewport's MaxZ 0.94");
        Check(wdl > 0.99f, "depth beyond the world range (distant terrain) counts as beyond the far clip");
        Image clientSkyT = renderDebugIn(2, 5000.0f, clientDepthRange, kClientWdlRawDepth);
        float distantTerrainT = clientSkyT.At(100, 50)[2] / 255.0f;
        float skyBesideTerrainT = clientSkyT.At(300, 50)[2] / 255.0f;
        std::printf("     transmittance: distant terrain %.3f, sky beside it %.3f\n", distantTerrainT,
                    skyBesideTerrainT);
        Check(distantTerrainT < skyBesideTerrainT - 0.1f, "distant terrain is fogged as terrain, not as sky");

        UINT midRow = 0;
        float midZ = 0.0f;
        for (UINT row = 687; row > 0 && !midRow; --row)
        {
            float rowNdc = 1.0f - (row + 0.5f) / 688.0f * 2.0f;
            float dz = rowNdc / proj[5] * view[9] + view[10];
            float z = dz < 0.0f ? -EyeHeightAboveGround(eye) / dz : 1e9f;
            if (z > 150.0f)
            {
                midRow = row;
                midZ = z;
            }
        }
        Image idealT = renderDebugIn(2, 5000.0f, world, kNoWdlPatch);
        Image clientT = renderDebugIn(2, 5000.0f, clientDepthRange, kNoWdlPatch);
        float ti = idealT.At(640, midRow)[2] / 255.0f;
        float tc = clientT.At(640, midRow)[2] / 255.0f;
        std::printf("     ground at %.0f yd: transmittance %.3f with the full depth range, %.3f with the client's\n",
                    midZ, ti, tc);
        Check(ti < 0.97f && std::fabs(ti - tc) < 2.5f / 255.0f,
              "fog on geometry is the same with the client's depth range");
    }

    {
        DWORD before = 0;
        DWORD forced = 0;
        DWORD during = 0;
        DWORD after = 0;
        h.dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        h.dev->GetRenderState(D3DRS_ZWRITEENABLE, &before);
        vf_test_force_depth_write(1);
        h.dev->GetRenderState(D3DRS_ZWRITEENABLE, &forced);
        h.dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        h.dev->GetRenderState(D3DRS_ZWRITEENABLE, &during);
        h.dev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        h.dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        vf_test_force_depth_write(0);
        h.dev->GetRenderState(D3DRS_ZWRITEENABLE, &after);
        Check(before == FALSE && forced == TRUE && during == TRUE && after == FALSE,
              "liquid-pass depth writes stay on while forced and the client's last request is restored");
    }

    Config restored = cfg;
    vf_test_set_config(&restored);

    CheckOverlayInput(h);
    CheckOverlayWidgets(h);
    CheckOverlayDraw(h, world, outDir);

    h.ReleaseEngineObjects();
    h.pp.BackBufferWidth = 1024;
    h.pp.BackBufferHeight = 600;
    hr = h.dev->Reset(&h.pp);
    Check(SUCCEEDED(hr), "Reset through the wrapper");
    h.dev->GetDepthStencilSurface(&depth);
    std::memset(&depthDesc, 0, sizeof(depthDesc));
    if (depth)
    {
        depth->GetDesc(&depthDesc);
        depth->Release();
    }
    Check(depthDesc.Format == kIntz && depthDesc.Width == 1024, "INTZ depth recreated at the new size after Reset");
    h.CreateEngineObjects();
    const D3DVIEWPORT9 resized = {0, 0, 1024, 600, 0.0f, 1.0f};
    EngineProjection(1024.0f / 600.0f, proj);
    CameraRelativeLookAt(eye, at, view);
    h.BeginFrame();
    h.DrawScene(eye, view, proj, resized);
    FrameInputs in = MakeInputs(view, proj, eye, at, resized);
    Check(vf_test_render(&in, &skip) != 0, (std::string("fog renders after Reset ") + skip).c_str());
    h.dev->EndScene();
    const Image beforeOverlay = Capture(h.dev);
    DrawOverlayFrames(kOverlaySettleFrames);
    Check(OverlayProbeChange(beforeOverlay, Capture(h.dev)) > 0.05, "the overlay draws again after Reset");
    PressHotkey(h.window, kDefaultOverlayHotkey);

    h.ReleaseEngineObjects();
    ULONG devRefs = h.dev->Release();
    ULONG d3dRefs = h.d3d->Release();
    Check(devRefs == 0 && d3dRefs == 0, "wrapper reference counts reach zero");
    DestroyWindow(h.window);
    CoUninitialize();
    std::printf("%s (%d failure%s)\n", g_failures ? "FAILED" : "OK", g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}

constexpr float kHarbourNear = 0.2f;
constexpr float kHarbourFar = 791.6f;
constexpr float kHarbourLoggedP11 = 1.511f;
constexpr UINT kHarbourWidth = 2560;
constexpr UINT kHarbourHeight = 1440;
constexpr int kHarbourSettleFrames = 24;
constexpr D3DCOLOR kHarbourSky = 0xFFFFD890;
constexpr D3DCOLOR kHarbourSea = 0xFF1A2430;
constexpr float kDegree = kPi / 180.0f;

Vec3 Dir(float azimuthDeg, float elevationDeg)
{
    float a = azimuthDeg * kDegree;
    float e = elevationDeg * kDegree;
    return {std::cos(e) * std::cos(a), std::cos(e) * std::sin(a), std::sin(e)};
}

D3DCOLOR Shade(D3DCOLOR c, float s)
{
    auto ch = [&](int shift) {
        return static_cast<D3DCOLOR>(std::fmin(((c >> shift) & 0xFF) * s, 255.0f)) << shift;
    };
    return 0xFF000000u | ch(16) | ch(8) | ch(0);
}

constexpr float kSeaLevel = 0.0f;

struct HarbourObject
{
    float degreesLeftOfView;
    float nearFaceDistance;
    float depth;
    float halfWidth;
    float topHeight;
    D3DCOLOR color;
};

constexpr HarbourObject kShip = {18.0f, 80.0f, 30.0f, 12.0f, 75.0f, 0xFF808080};
constexpr HarbourObject kLighthouse = {-6.0f, 350.0f, 20.0f, 10.0f, 140.0f, 0xFFA8A8B0};
constexpr HarbourObject kRidge = {-25.0f, 650.0f, 80.0f, 150.0f, 190.0f, 0xFF4A5A38};
const HarbourObject kHarbourObjects[] = {kShip, kLighthouse, kRidge};

void AddBoxStandingOnSea(std::vector<SceneVertex>& v, float azimuthDeg, const HarbourObject& o)
{
    Vec3 forward = Dir(azimuthDeg, 0.0f);
    Vec3 right = {forward.y, -forward.x, 0.0f};
    Vec3 nearFace = {kHarbourEye.x + forward.x * o.nearFaceDistance, kHarbourEye.y + forward.y * o.nearFaceDistance,
                     kSeaLevel};
    auto corner = [&](float along, float side, float z) {
        return Vec3{nearFace.x + forward.x * along + right.x * side, nearFace.y + forward.y * along + right.y * side,
                    z};
    };
    const float w = o.halfWidth;
    const float top = o.topHeight;
    Vec3 p[8] = {corner(0, -w, kSeaLevel), corner(0, w, kSeaLevel), corner(o.depth, w, kSeaLevel),
                 corner(o.depth, -w, kSeaLevel), corner(0, -w, top), corner(0, w, top), corner(o.depth, w, top),
                 corner(o.depth, -w, top)};
    const Vec3 absolute = {0, 0, 0};
    AddQuad(v, p[0], p[1], p[5], p[4], o.color, absolute);
    AddQuad(v, p[1], p[2], p[6], p[5], Shade(o.color, 0.75f), absolute);
    AddQuad(v, p[2], p[3], p[7], p[6], Shade(o.color, 0.6f), absolute);
    AddQuad(v, p[3], p[0], p[4], p[7], Shade(o.color, 0.75f), absolute);
    AddQuad(v, p[4], p[5], p[6], p[7], Shade(o.color, 1.15f), absolute);
}

std::vector<SceneVertex> BuildHarbourSceneFacing(const float* viewAzimuths, int viewCount)
{
    std::vector<SceneVertex> v;
    const float e = 3000.0f;
    const Vec3 absolute = {0, 0, 0};
    const Vec3 o = {kHarbourEye.x, kHarbourEye.y, kSeaLevel};
    AddQuad(v, Add(o, {-e, -e, 0}), Add(o, {e, -e, 0}), Add(o, {e, e, 0}), Add(o, {-e, e, 0}), kHarbourSea, absolute);
    for (int i = 0; i < viewCount; ++i)
        for (const HarbourObject& obj : kHarbourObjects)
            AddBoxStandingOnSea(v, viewAzimuths[i] + obj.degreesLeftOfView, obj);
    return v;
}

enum class ProbeKind
{
    World,
    Sky,
    SunAzimuth,
};

struct HarbourProbe
{
    const char* name;
    ProbeKind kind;
    float degreesLeftOfView;
    float horizontalDistance;
    float height;
    float elevationDeg;
    float elevationFromSunDeg;
};

HarbourProbe WorldProbe(const char* name, float degreesLeftOfView, float horizontalDistance, float height)
{
    return {name, ProbeKind::World, degreesLeftOfView, horizontalDistance, height, 0.0f, 0.0f};
}

HarbourProbe SkyProbe(const char* name, float degreesLeftOfView, float elevationDeg)
{
    return {name, ProbeKind::Sky, degreesLeftOfView, 0.0f, 0.0f, elevationDeg, 0.0f};
}

HarbourProbe SunAzimuthProbe(const char* name, float elevationFromSunDeg)
{
    return {name, ProbeKind::SunAzimuth, 0.0f, 0.0f, 0.0f, 0.0f, elevationFromSunDeg};
}

const HarbourProbe kHarbourProbes[] = {
    WorldProbe("ship (80 yd, z 55)", 18.0f, 80.0f, 55.0f),
    WorldProbe("ship hull (80 yd, z 10)", 18.0f, 80.0f, 10.0f),
    WorldProbe("lighthouse (350 yd, z 60)", -6.0f, 350.0f, 60.0f),
    WorldProbe("lighthouse top (350 yd, z 125)", -6.0f, 350.0f, 125.0f),
    WorldProbe("ridge (650 yd, z 60)", -25.0f, 650.0f, 60.0f),
    WorldProbe("ridge top (650 yd, z 170)", -25.0f, 650.0f, 170.0f),
    WorldProbe("sea 100 yd", 3.0f, 100.0f, kSeaLevel),
    WorldProbe("sea 160 yd", 3.0f, 160.0f, kSeaLevel),
    WorldProbe("sea 400 yd", 3.0f, 400.0f, kSeaLevel),
    WorldProbe("sea 700 yd", 3.0f, 700.0f, kSeaLevel),
    SkyProbe("sky past the far clip (-4 deg)", 3.0f, -4.0f),
    SkyProbe("sky at the horizon (+1 deg)", 30.0f, 1.0f),
    SkyProbe("sky 10 deg", 30.0f, 10.0f),
    SkyProbe("sky 40 deg", 30.0f, 40.0f),
    SunAzimuthProbe("near the sun (6 deg below)", -6.0f),
    SunAzimuthProbe("near the sun (15 deg below)", -15.0f),
};

Vec3 ProbeOffsetFromEye(const HarbourProbe& p, float viewAzimuth, float sunAzimuth, float sunElevation)
{
    if (p.kind == ProbeKind::World)
    {
        Vec3 q = Dir(viewAzimuth + p.degreesLeftOfView, 0.0f);
        return {q.x * p.horizontalDistance, q.y * p.horizontalDistance, p.height - kHarbourEye.z};
    }
    if (p.kind == ProbeKind::Sky)
        return Dir(viewAzimuth + p.degreesLeftOfView, p.elevationDeg);
    return Dir(sunAzimuth, sunElevation + p.elevationFromSunDeg);
}

struct HarbourView
{
    const wchar_t* file;
    const char* name;
    float baseAzimuth;
    float pitch;
    float worldViewportMaxZ;
};

constexpr float kMaxDistanceBelowHarbourFarClip = 200.0f;
constexpr float kProbeDepthToleranceYards = 6.0f;

float DepthViewYards(const Image& depth, int x, int y)
{
    return depth.At(x, y)[2] / 255.0f * kHarbourFar;
}

bool DepthViewIsSky(const Image& depth, int x, int y)
{
    return depth.At(x, y)[2] >= 254;
}

struct Rgb
{
    float r, g, b;
};

Rgb SampleRgb(const Image& img, int cx, int cy, int radius)
{
    Rgb sum = {};
    int n = 0;
    for (int y = cy - radius; y <= cy + radius; ++y)
        for (int x = cx - radius; x <= cx + radius; ++x)
        {
            const unsigned char* p = img.At(static_cast<UINT>(x), static_cast<UINT>(y));
            sum.r += p[2];
            sum.g += p[1];
            sum.b += p[0];
            ++n;
        }
    return {sum.r / n, sum.g / n, sum.b / n};
}

void ReferenceUnshadowedLayerOpticalDepths(const FogParams& fog, float camZ, Vec3 dirW, float viewZ, float rayLen,
                                           bool sky, float* tau)
{
    float z = sky ? fog.maxDistance : viewZ;
    float horizon = sky ? 1.0f : SmoothStep(fog.horizonStart, fog.farClip, z);
    z = z + (fog.maxDistance - z) * horizon;
    float tMax = std::fmin(z * rayLen, fog.maxDistance);
    float up = std::fmax(dirW.z, 0.0f);
    float dirZ = dirW.z + (up - dirW.z) * horizon;
    const float dt = tMax / kReferenceIntegrationSteps;
    for (int j = 0; j < kFogLayers; ++j)
        tau[j] = 0.0f;
    for (int i = 0; i < kReferenceIntegrationSteps; ++i)
    {
        float t = (i + 0.5f) * dt;
        float h = camZ + dirZ * t;
        for (int j = 0; j < kFogLayers; ++j)
        {
            const FogLayer& l = fog.layers[j];
            if (t < l.start || t > l.endDistance)
                continue;
            float scale = sky ? std::exp(-up * l.skyFalloff) : 1.0f;
            float curve = 1.0f + l.strength * std::pow(std::fmin(std::fmax(t - l.start, 0.0f) / fog.maxDistance, 1.0f) +
                                                           1e-6f,
                                                       l.exponent);
            float heightF = std::fmin(std::exp((l.upperHeight - h) * l.upperFalloff), 1.0f) *
                            std::fmin(std::exp((h - l.lowerHeight) * l.lowerFalloff), 1.0f);
            tau[j] += l.density * scale * curve * heightF * dt;
        }
    }
}

void PrintLayers(const FogParams& fog)
{
    for (int i = 0; i < kFogLayers; ++i)
    {
        const FogLayer& l = fog.layers[i];
        std::printf("  layer %d (%s): start %.0f density %.6f g %.2f diffuse %.2f %.2f %.2f emissive %.2f %.2f %.2f "
                    "upper %.1f/%.4f lower %.1f/%.4f shadowed %.0f limit %.0f\n",
                    i, fog.authored && i < kSceneLayers ? "classic" : "derived", l.start, l.density, l.g, l.diffuse[0],
                    l.diffuse[1], l.diffuse[2], l.emissive[0], l.emissive[1], l.emissive[2], l.upperHeight,
                    l.upperFalloff, l.lowerHeight, l.lowerFalloff, l.shadowed, std::fmin(l.endDistance, 99999.0f));
        std::printf("           curve strength %.2f exponent %.2f, sky falloff %.2f, shadow density %.2f, "
                    "shadow emissive %.2f %.2f %.2f, isotropic %.2f\n",
                    l.strength, l.exponent, l.skyFalloff, l.shadowDensity, l.shadowEmissive[0], l.shadowEmissive[1],
                    l.shadowEmissive[2], l.isotropic);
    }
}

int RunHarbour(const std::wstring& outDir, const std::string& dataPath)
{
    FogData classic;
    if (!classic.Load(dataPath))
    {
        std::printf("Classic fog data %s did not load\n", dataPath.c_str());
        return 1;
    }
    CreateDirectoryW(outDir.c_str(), nullptr);
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    const Vec3 toLight = Norm(kHarbourToLight);
    const float sunAz = std::atan2(toLight.y, toLight.x) / kDegree;
    const float sunEl = std::asin(toLight.z) / kDegree;
    const float pitch = sunEl - 20.0f;
    const HarbourView views[] = {
        {L"sun", "facing the sun, sun 20 deg above the view centre", sunAz, pitch, kIdealWorldMaxZ},
        {L"right", "90 deg right of the sun, same pitch", sunAz - 90.0f, pitch, kIdealWorldMaxZ},
        {L"sun-up", "facing the sun, pitched up 25 deg (high sky)", sunAz, 25.0f, kIdealWorldMaxZ},
        {L"right-up", "90 deg right of the sun, pitched up 25 deg (high sky)", sunAz - 90.0f, 25.0f, kIdealWorldMaxZ},
        {L"sun-down", "facing the sun, pitched down 40 deg (ship hull, near sea)", sunAz, -40.0f, kIdealWorldMaxZ},
        {L"sun-client-depth-range", "facing the sun, world drawn with the client's viewport MaxZ 0.94", sunAz, pitch,
         kClientWorldMaxZ},
        {L"right-client-depth-range", "90 deg right, world drawn with the client's viewport MaxZ 0.94",
         sunAz - 90.0f, pitch, kClientWorldMaxZ},
    };
    const float baseAzimuths[2] = {sunAz, sunAz - 90.0f};

    WNDCLASSW wc = {};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"vfog_harbour";
    RegisterClassW(&wc);
    Harness h;
    h.window = CreateWindowW(L"vfog_harbour", L"vfog harbour", WS_OVERLAPPEDWINDOW, 0, 0, 1280, 720, nullptr, nullptr,
                             wc.hInstance, nullptr);
    HMODULE d3d9 = LoadLibraryA("d3d9.dll");
    auto realCreate = reinterpret_cast<IDirect3D9*(WINAPI*)(UINT)>(GetProcAddress(d3d9, "Direct3DCreate9"));
    h.d3d = vf_test_wrap_direct3d9(realCreate, D3D_SDK_VERSION);
    if (!h.d3d)
        return 1;
    h.pp.Windowed = TRUE;
    h.pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    h.pp.BackBufferWidth = kHarbourWidth;
    h.pp.BackBufferHeight = kHarbourHeight;
    h.pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    h.pp.EnableAutoDepthStencil = TRUE;
    h.pp.AutoDepthStencilFormat = D3DFMT_D24S8;
    h.pp.hDeviceWindow = h.window;
    h.pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    DWORD engineFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE | D3DCREATE_FPU_PRESERVE;
    if (FAILED(h.d3d->CreateDevice(0, D3DDEVTYPE_HAL, h.window, engineFlags, &h.pp, &h.dev)) || !h.dev)
    {
        std::printf("CreateDevice through the wrapper failed\n");
        h.d3d->Release();
        return 1;
    }
    h.scene = BuildHarbourSceneFacing(baseAzimuths, 2);
    h.clearColor = kHarbourSky;

    const D3DVIEWPORT9 vp = {0, 0, kHarbourWidth, kHarbourHeight, 0.0f, 1.0f};
    const float aspect = static_cast<float>(kHarbourWidth) / kHarbourHeight;
    float proj[16];
    EngineGlDepthProjection(kHarbourLoggedP11, aspect, kHarbourNear, kHarbourFar, proj);
    const Config shippedCfg = {};

    auto inputsFor = [&](const float* view, Vec3 at) {
        FrameInputs in = MakeInputs(view, proj, kHarbourEye, at, vp);
        Vec3 clientTargetOneYardAhead = Add(kHarbourEye, Norm(Sub(at, kHarbourEye)));
        in.camTarget[0] = clientTargetOneYardAhead.x;
        in.camTarget[1] = clientTargetOneYardAhead.y;
        in.camTarget[2] = clientTargetOneYardAhead.z;
        in.dayFraction = kHarbourDayFraction;
        in.toLight[0] = toLight.x;
        in.toLight[1] = toLight.y;
        in.toLight[2] = toLight.z;
        in.lightIsMoon = false;
        in.fogColor = 0xFF574C5C;
        in.sunColor = 0xFFFFE7B6;
        in.directColor = 0xFFFF7400;
        in.ambientColor = 0xFF676680;
        in.fogStart = kContinentFogStart;
        in.fogEnd = 791.7f;
        in.zoneFogDistance = 791.7f;
        in.farClip = kHarbourFar;
        in.inLiquid = false;
        in.mapId = kEasternKingdoms;
        return in;
    };

    {
        float view[16];
        Vec3 at = Add(kHarbourEye, Dir(views[0].baseAzimuth, views[0].pitch));
        CameraRelativeLookAt(kHarbourEye, at, view);
        FrameInputs in = inputsFor(view, at);
        AuthoredFog authored = {};
        bool resolved = classic.Resolve(kEasternKingdoms, in.camPos, in.dayFraction, kClearWeather, authored);
        std::printf("harbour frame: camera (%.1f %.1f %.1f), day %.4f, toLight (%.3f %.3f %.3f) = azimuth %.1f "
                    "elevation %.2f deg\n",
                    in.camPos[0], in.camPos[1], in.camPos[2], in.dayFraction, toLight.x, toLight.y, toLight.z, sunAz,
                    sunEl);
        std::printf("  projection P00 %.4f P11 %.4f near %.2f far %.1f, %ux%u\n", proj[0], proj[5], kHarbourNear,
                    kHarbourFar, kHarbourWidth, kHarbourHeight);
        std::printf("  Classic lights:");
        for (int i = 0; i < authored.lightCount; ++i)
            std::printf(" %u:%.2f", authored.lightIds[i], authored.lightWeights[i]);
        std::printf(" (%s, %d layers)\n", resolved ? "resolved" : "NOT resolved", authored.layerCount);
        for (int i = 0; i < authored.layerCount; ++i)
        {
            const AuthoredLayer& a = authored.layers[i];
            std::printf("  authored %d: start %.1f density %.4f g %.3f intensity %.2f strength %.3f exponent %.3f "
                        "diffuse %.3f %.3f %.3f emissive %.3f %.3f %.3f flags %u\n",
                        i, a.start, a.density, a.g, a.intensity, a.strength, a.exponent, a.diffuse[0], a.diffuse[1],
                        a.diffuse[2], a.emissive[0], a.emissive[1], a.emissive[2], a.flags);
        }
        FogParams fog = BuildFogParams(in, shippedCfg, resolved ? &authored : nullptr);
        std::printf("  fog params (as the DLL logs them): refZ %.1f maxDistance %.0f horizonStart %.1f farLimit %.1f "
                    "exposure %.2f\n",
                    fog.referenceZ, fog.maxDistance, fog.horizonStart, fog.farLimit,
                    fog.authored ? shippedCfg.classicExposure : shippedCfg.exposure);
        PrintLayers(fog);
    }

    const char* skip = "";
    auto frame = [&](const float* view, const FrameInputs& in, const Config& c, bool capture, Image* before) {
        vf_test_set_config(&c);
        h.BeginFrame();
        h.DrawScene(kHarbourEye, view, proj, in.viewport);
        if (before)
            *before = Capture(h.dev);
        if (!vf_test_render(&in, &skip))
            std::printf("     fog skipped: %s\n", skip);
        Image img;
        if (capture)
            img = Capture(h.dev);
        h.dev->EndScene();
        h.dev->Present(nullptr, nullptr, nullptr, nullptr);
        return img;
    };
    auto restartTemporalHistory = [&](const float* view, const FrameInputs& in) {
        Config withoutHistory = shippedCfg;
        withoutHistory.temporal = 0.0f;
        frame(view, in, withoutHistory, false, nullptr);
    };

    for (const HarbourView& hv : views)
    {
        float view[16];
        Vec3 at = Add(kHarbourEye, Dir(hv.baseAzimuth, hv.pitch));
        CameraRelativeLookAt(kHarbourEye, at, view);
        FrameInputs in = inputsFor(view, at);
        in.viewport.MaxZ = hv.worldViewportMaxZ;
        const bool linearDepthComparable = hv.worldViewportMaxZ >= kIdealWorldMaxZ;

        Config depthCfg = shippedCfg;
        depthCfg.debugView = 3;
        depthCfg.maxDistance = kMaxDistanceBelowHarbourFarClip;
        depthCfg.temporal = 0.0f;
        Image depth = frame(view, in, depthCfg, true, nullptr);

        restartTemporalHistory(view, in);
        Config c = shippedCfg;
        Image before;
        Image after;
        for (int i = 1; i < kHarbourSettleFrames; ++i)
        {
            bool last = i == kHarbourSettleFrames - 1;
            Image img = frame(view, in, c, last, last ? &before : nullptr);
            if (last)
                after = img;
        }
        Image radiance;
        Image transmittance;
        c.debugView = 1;
        for (int i = 0; i < 4; ++i)
            radiance = frame(view, in, c, i == 3, nullptr);
        c.debugView = 2;
        for (int i = 0; i < 4; ++i)
            transmittance = frame(view, in, c, i == 3, nullptr);

        std::wstring stem = outDir + L"\\harbour-" + hv.file;
        SavePng(stem + L"-before.png", before.w, before.h, before.bgra);
        SavePng(stem + L"-after.png", after.w, after.h, after.bgra);
        SavePng(stem + L"-radiance.png", radiance.w, radiance.h, radiance.bgra);
        SavePng(stem + L"-transmittance.png", transmittance.w, transmittance.h, transmittance.bgra);
        SavePng(stem + L"-depth.png", depth.w, depth.h, depth.bgra);

        AuthoredFog authored = {};
        bool resolved = classic.Resolve(kEasternKingdoms, in.camPos, in.dayFraction, kClearWeather, authored);
        FogParams fog = BuildFogParams(in, shippedCfg, resolved ? &authored : nullptr);
        float sunV[3];
        TransformDirection(in.toLight, view, sunV);
        std::printf("\nview %ls: %s (azimuth %.1f, pitch %.1f; sun view-space %.3f %.3f %.3f, refZ %.1f, "
                    "viewport MaxZ %.2f)\n",
                    hv.file, hv.name, hv.baseAzimuth, hv.pitch, sunV[0], sunV[1], sunV[2], fog.referenceZ,
                    hv.worldViewportMaxZ);
        std::printf("  %-31s %6s %6s %6s %13s %6s %15s %15s %15s %15s %6s  %s\n", "probe", "px", "py", "dist",
                    "viewZ/shader", "alpha", "fog rgb", "fog rgb / a", "before", "after", "cpu a",
                    "cpu tau L0 L1 L2 L3");
        for (const HarbourProbe& p : kHarbourProbes)
        {
            Vec3 d = ProbeOffsetFromEye(p, hv.baseAzimuth, sunAz, sunEl);
            float vx = d.x * view[0] + d.y * view[4] + d.z * view[8];
            float vy = d.x * view[1] + d.y * view[5] + d.z * view[9];
            float vz = d.x * view[2] + d.y * view[6] + d.z * view[10];
            if (vz <= 0.01f)
                continue;
            float px = (vx / vz * proj[0] + proj[8]) * 0.5f + 0.5f;
            float py = 0.5f - (vy / vz * proj[5] + proj[9]) * 0.5f;
            int ix = static_cast<int>(px * kHarbourWidth);
            int iy = static_cast<int>(py * kHarbourHeight);
            if (ix < 4 || iy < 4 || ix >= static_cast<int>(kHarbourWidth) - 4 ||
                iy >= static_cast<int>(kHarbourHeight) - 4)
                continue;
            const bool world = p.kind == ProbeKind::World;
            float len = std::sqrt(Dot(d, d));
            float zShader = DepthViewYards(depth, ix, iy);
            bool shaderSky = DepthViewIsSky(depth, ix, iy);
            bool depthAgrees = !linearDepthComparable || std::fabs(zShader - vz) < kProbeDepthToleranceYards;
            bool matches = world ? depthAgrees && !shaderSky : shaderSky;
            if (!matches)
            {
                std::printf("  %-31s %6d %6d  hidden (depth %.1f yd%s)\n", p.name, ix, iy, zShader,
                            shaderSky ? ", sky" : "");
                continue;
            }
            const int radius = 2;
            Rgb fogRgb = SampleRgb(radiance, ix, iy, radius);
            Rgb t = SampleRgb(transmittance, ix, iy, radius);
            Rgb b = SampleRgb(before, ix, iy, radius);
            Rgb a = SampleRgb(after, ix, iy, radius);
            float alpha = 1.0f - t.r / 255.0f;
            float inv = alpha > 0.02f ? 1.0f / alpha : 0.0f;
            float tau[kFogLayers];
            Vec3 dirW = {d.x / len, d.y / len, d.z / len};
            ReferenceUnshadowedLayerOpticalDepths(fog, kHarbourEye.z, dirW, world ? vz : 0.0f, world ? len / vz : 1.0f,
                                                  !world, tau);
            float tauSum = tau[0] + tau[1] + tau[2] + tau[3];
            char distText[16];
            char zText[24];
            if (world)
            {
                std::snprintf(distText, sizeof(distText), "%.0f", len);
                std::snprintf(zText, sizeof(zText), "%.0f/%.0f", vz, zShader);
            }
            else
            {
                std::snprintf(distText, sizeof(distText), "sky");
                std::snprintf(zText, sizeof(zText), "sky/sky");
            }
            std::printf("  %-31s %6d %6d %6s %13s %6.3f %4.0f %4.0f %4.0f  %4.0f %4.0f %4.0f  %4.0f %4.0f %4.0f  "
                        "%4.0f %4.0f %4.0f %6.3f  %.3f %.3f %.3f %.3f\n",
                        p.name, ix, iy, distText, zText, alpha, fogRgb.r, fogRgb.g, fogRgb.b,
                        std::fmin(fogRgb.r * inv, 999.0f), std::fmin(fogRgb.g * inv, 999.0f),
                        std::fmin(fogRgb.b * inv, 999.0f), b.r, b.g, b.b, a.r, a.g, a.b, 1.0f - std::exp(-tauSum),
                        tau[0], tau[1], tau[2], tau[3]);
        }
    }

    vf_test_set_config(&shippedCfg);
    ULONG devRefs = h.dev->Release();
    ULONG d3dRefs = h.d3d->Release();
    DestroyWindow(h.window);
    CoUninitialize();
    std::printf("\nharbour scene written to %ls (device refs %lu, d3d refs %lu)\n", outDir.c_str(), devRefs, d3dRefs);
    return 0;
}
}

int wmain(int argc, wchar_t** argv)
{
    std::wstring out = L"harness-out";
    std::string data = "fogdata.bin";
    std::wstring ini = L"CoAVolFog.ini";
    std::wstring scene;
    for (int i = 1; i + 1 < argc; ++i)
    {
        if (std::wcscmp(argv[i], L"--out") == 0)
            out = argv[i + 1];
        if (std::wcscmp(argv[i], L"--data") == 0)
        {
            char path[MAX_PATH] = {};
            WideCharToMultiByte(CP_ACP, 0, argv[i + 1], -1, path, MAX_PATH, nullptr, nullptr);
            data = path;
        }
        if (std::wcscmp(argv[i], L"--ini") == 0)
            ini = argv[i + 1];
        if (std::wcscmp(argv[i], L"--scene") == 0)
        {
            scene = argv[i + 1];
            if (i + 2 < argc && argv[i + 2][0] != L'-')
                out = argv[i + 2];
        }
    }
    if (scene == L"harbour")
        return RunHarbour(out, data);
    if (!scene.empty())
    {
        std::printf("unknown scene %ls (known: harbour)\n", scene.c_str());
        return 2;
    }
    return Run(out, data, ini);
}
