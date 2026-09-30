#include "grading_renderer.h"

#include "log.h"

#include "ps_grade.h"
#include "vs_fullscreen.h"

#include <cstring>

namespace
{
constexpr UINT kGradingConstants = 2;
constexpr DWORD kGradingStages = 2;
constexpr DWORD kSceneStage = 0;
constexpr DWORD kCurveStage = 1;
constexpr DWORD kColourChannels = D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE;
constexpr DWORD kEverySample = 0xFFFFFFFF;
constexpr float kFullscreenTriangleLow = -1.5f;
constexpr float kFullscreenTriangleHigh = 4.5f;

struct RenderStateValue
{
    D3DRENDERSTATETYPE state;
    DWORD value;
};

const RenderStateValue kGradingRenderStates[] = {
    {D3DRS_ZENABLE, D3DZB_FALSE},
    {D3DRS_ZWRITEENABLE, FALSE},
    {D3DRS_STENCILENABLE, FALSE},
    {D3DRS_TWOSIDEDSTENCILMODE, FALSE},
    {D3DRS_ALPHATESTENABLE, FALSE},
    {D3DRS_ALPHABLENDENABLE, FALSE},
    {D3DRS_SEPARATEALPHABLENDENABLE, FALSE},
    {D3DRS_FOGENABLE, FALSE},
    {D3DRS_SRGBWRITEENABLE, FALSE},
    {D3DRS_CULLMODE, D3DCULL_NONE},
    {D3DRS_FILLMODE, D3DFILL_SOLID},
    {D3DRS_CLIPPLANEENABLE, 0},
    {D3DRS_SCISSORTESTENABLE, TRUE},
    {D3DRS_COLORWRITEENABLE, kColourChannels},
    {D3DRS_MULTISAMPLEMASK, kEverySample},
};

struct SamplerStateValue
{
    D3DSAMPLERSTATETYPE state;
    DWORD value;
};

const SamplerStateValue kPointSampledClamp[] = {
    {D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP}, {D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP},
    {D3DSAMP_MAGFILTER, D3DTEXF_POINT},    {D3DSAMP_MINFILTER, D3DTEXF_POINT},
    {D3DSAMP_MIPFILTER, D3DTEXF_NONE},     {D3DSAMP_SRGBTEXTURE, FALSE},
    {D3DSAMP_MAXMIPLEVEL, 0},
};

const D3DFORMAT kCurveFormats[] = {D3DFMT_R32F, D3DFMT_A32B32G32R32F};

template <typename T>
void SafeRelease(T*& p)
{
    if (p)
    {
        p->Release();
        p = nullptr;
    }
}

RECT ViewportRect(const D3DVIEWPORT9& vp)
{
    return {static_cast<LONG>(vp.X), static_cast<LONG>(vp.Y), static_cast<LONG>(vp.X + vp.Width),
            static_cast<LONG>(vp.Y + vp.Height)};
}

bool InsideTarget(const D3DVIEWPORT9& world, const D3DSURFACE_DESC& desc)
{
    return world.Width > 0 && world.Height > 0 && world.X + world.Width <= desc.Width &&
           world.Y + world.Height <= desc.Height;
}

UINT FloatsPerCurveTexel(D3DFORMAT format)
{
    return format == D3DFMT_A32B32G32R32F ? 4u : 1u;
}

void BindPointSampled(IDirect3DDevice9* dev, DWORD stage, IDirect3DBaseTexture9* texture)
{
    dev->SetTexture(stage, texture);
    for (const SamplerStateValue& sampler : kPointSampledClamp)
        dev->SetSamplerState(stage, sampler.state, sampler.value);
}

void DrawFullscreenTriangle(IDirect3DDevice9* dev)
{
    static const float kTriangle[3][4] = {{kFullscreenTriangleLow, kFullscreenTriangleLow, 0.0f, 1.0f},
                                          {kFullscreenTriangleLow, kFullscreenTriangleHigh, 0.0f, 1.0f},
                                          {kFullscreenTriangleHigh, kFullscreenTriangleLow, 0.0f, 1.0f}};
    dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, kTriangle, sizeof(kTriangle[0]));
}
}

GradingRenderer::~GradingRenderer()
{
    ReleaseAll();
}

void GradingRenderer::ReleaseDefaultPool()
{
    SafeRelease(m_sceneCopy);
    m_sceneCopyWidth = m_sceneCopyHeight = 0;
    m_sceneCopyFormat = D3DFMT_UNKNOWN;
    m_sceneCopyFailed = false;
    SafeRelease(m_state);
}

void GradingRenderer::ReleaseAll()
{
    ReleaseDefaultPool();
    SafeRelease(m_curve);
    m_curveFormat = D3DFMT_UNKNOWN;
    m_curveUploaded = false;
    SafeRelease(m_vs);
    SafeRelease(m_ps);
    SafeRelease(m_decl);
    m_unsupportedDevice = nullptr;
}

GradingStats GradingRenderer::Stats() const
{
    GradingStats stats;
    stats.grades = m_grades;
    stats.curveUploads = m_curveUploads;
    stats.holdsSceneCopy = m_sceneCopy != nullptr;
    stats.holdsCurve = m_curve != nullptr;
    return stats;
}

bool GradingRenderer::Skip(const char* reason)
{
    m_skip = reason;
    return false;
}

bool GradingRenderer::EnsureShaders(IDirect3DDevice9* dev)
{
    if (m_unsupportedDevice == dev)
        return Skip("grading shader unsupported");
    if (m_ps)
        return true;
    static const D3DVERTEXELEMENT9 kElements[] = {
        {0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        D3DDECL_END(),
    };
    HRESULT result = dev->CreateVertexShader(reinterpret_cast<const DWORD*>(g_vs_fullscreen), &m_vs);
    if (SUCCEEDED(result))
        result = dev->CreatePixelShader(reinterpret_cast<const DWORD*>(g_ps_grade), &m_ps);
    if (SUCCEEDED(result))
        result = dev->CreateVertexDeclaration(kElements, &m_decl);
    if (SUCCEEDED(result))
        return true;
    SafeRelease(m_vs);
    SafeRelease(m_ps);
    SafeRelease(m_decl);
    m_unsupportedDevice = dev;
    VF_LOG_ERROR("colour grading shaders could not be created (HRESULT 0x%08lX); no grading on this device",
                 static_cast<unsigned long>(result));
    return Skip("grading shader unsupported");
}

bool GradingRenderer::EnsureStateBlock(IDirect3DDevice9* dev)
{
    if (m_state)
        return true;
    if (FAILED(dev->BeginStateBlock()))
        return Skip("state block recording failed");
    for (const RenderStateValue& renderState : kGradingRenderStates)
        dev->SetRenderState(renderState.state, 0);
    for (DWORD stage = 0; stage < kGradingStages; ++stage)
    {
        dev->SetTexture(stage, nullptr);
        for (const SamplerStateValue& sampler : kPointSampledClamp)
            dev->SetSamplerState(stage, sampler.state, 0);
    }
    const float zeros[kGradingConstants * 4] = {};
    dev->SetVertexShader(nullptr);
    dev->SetPixelShader(nullptr);
    dev->SetPixelShaderConstantF(0, zeros, kGradingConstants);
    dev->SetVertexDeclaration(m_decl);
    dev->SetStreamSource(0, nullptr, 0, 0);
    D3DVIEWPORT9 viewport = {0, 0, 1, 1, 0.0f, 1.0f};
    dev->SetViewport(&viewport);
    RECT scissor = {0, 0, 1, 1};
    dev->SetScissorRect(&scissor);
    if (FAILED(dev->EndStateBlock(&m_state)) || !m_state)
    {
        m_state = nullptr;
        return Skip("state block recording failed");
    }
    return true;
}

bool GradingRenderer::EnsureCurve(IDirect3DDevice9* dev, const float* curve)
{
    if (!m_curve)
    {
        for (D3DFORMAT format : kCurveFormats)
        {
            if (SUCCEEDED(dev->CreateTexture(kGradingCurveEntries, 1, 1, 0, format, D3DPOOL_MANAGED, &m_curve,
                                             nullptr)) &&
                m_curve)
            {
                m_curveFormat = format;
                break;
            }
            m_curve = nullptr;
        }
        if (!m_curve)
            return Skip("curve texture creation failed");
        m_curveUploaded = false;
    }
    if (m_curveUploaded && std::memcmp(m_uploadedCurve, curve, sizeof(m_uploadedCurve)) == 0)
        return true;
    return UploadCurve(curve) || Skip("curve texture upload failed");
}

bool GradingRenderer::UploadCurve(const float* curve)
{
    D3DLOCKED_RECT locked = {};
    if (FAILED(m_curve->LockRect(0, &locked, nullptr, 0)))
        return false;
    auto* texels = static_cast<float*>(locked.pBits);
    const UINT floats = FloatsPerCurveTexel(m_curveFormat);
    for (int entry = 0; entry < kGradingCurveEntries; ++entry)
        for (UINT channel = 0; channel < floats; ++channel)
            texels[entry * floats + channel] = curve[entry];
    m_curve->UnlockRect(0);
    std::memcpy(m_uploadedCurve, curve, sizeof(m_uploadedCurve));
    m_curveUploaded = true;
    ++m_curveUploads;
    return true;
}

bool GradingRenderer::EnsureSceneCopy(IDirect3DDevice9* dev, D3DFORMAT format, UINT width, UINT height)
{
    if (m_sceneCopy && m_sceneCopyWidth == width && m_sceneCopyHeight == height && m_sceneCopyFormat == format)
        return true;
    SafeRelease(m_sceneCopy);
    m_sceneCopyWidth = m_sceneCopyHeight = 0;
    if (m_sceneCopyFailed)
        return Skip("scene copy creation failed");
    const D3DFORMAT candidates[] = {format, D3DFMT_A8R8G8B8};
    for (D3DFORMAT candidate : candidates)
        if (SUCCEEDED(dev->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, candidate, D3DPOOL_DEFAULT,
                                         &m_sceneCopy, nullptr)) &&
            m_sceneCopy)
        {
            m_sceneCopyWidth = width;
            m_sceneCopyHeight = height;
            m_sceneCopyFormat = format;
            return true;
        }
    m_sceneCopy = nullptr;
    m_sceneCopyFailed = true;
    VF_LOG_ERROR("colour grading scene copy creation failed (%ux%u)", width, height);
    return Skip("scene copy creation failed");
}

bool GradingRenderer::CopyWorld(IDirect3DDevice9* dev, IDirect3DSurface9* target, const D3DSURFACE_DESC& desc,
                                const D3DVIEWPORT9& world)
{
    if (!EnsureSceneCopy(dev, desc.Format, world.Width, world.Height))
        return false;
    const RECT rect = ViewportRect(world);
    IDirect3DSurface9* copy = nullptr;
    const bool copied = SUCCEEDED(m_sceneCopy->GetSurfaceLevel(0, &copy)) &&
                        SUCCEEDED(dev->StretchRect(target, &rect, copy, nullptr, D3DTEXF_POINT));
    SafeRelease(copy);
    return copied || Skip("scene copy failed");
}

void GradingRenderer::SetGradingState(IDirect3DDevice9* dev, const D3DVIEWPORT9& world, float strength)
{
    for (const RenderStateValue& renderState : kGradingRenderStates)
        dev->SetRenderState(renderState.state, renderState.value);
    const RECT scissor = ViewportRect(world);
    dev->SetViewport(&world);
    dev->SetScissorRect(&scissor);
    dev->SetVertexShader(m_vs);
    dev->SetPixelShader(m_ps);
    dev->SetVertexDeclaration(m_decl);
    BindPointSampled(dev, kSceneStage, m_sceneCopy);
    BindPointSampled(dev, kCurveStage, m_curve);
    const float constants[kGradingConstants][4] = {
        {static_cast<float>(world.X), static_cast<float>(world.Y), 1.0f / world.Width, 1.0f / world.Height},
        {strength, 0.0f, 0.0f, 0.0f},
    };
    dev->SetPixelShaderConstantF(0, &constants[0][0], kGradingConstants);
}

void GradingRenderer::DrawGraded(IDirect3DDevice9* dev, const BoundTargets& bound, const D3DVIEWPORT9& world,
                                 float strength)
{
    IDirect3DVertexBuffer9* stream = nullptr;
    UINT streamOffset = 0;
    UINT streamStride = 0;
    dev->GetStreamSource(0, &stream, &streamOffset, &streamStride);
    m_state->Capture();
    for (DWORD i = 1; i < kRenderTargets; ++i)
        if (bound.colour[i])
            dev->SetRenderTarget(i, nullptr);
    dev->SetDepthStencilSurface(nullptr);
    SetGradingState(dev, world, strength);
    DrawFullscreenTriangle(dev);
    for (DWORD i = 1; i < kRenderTargets; ++i)
        if (bound.colour[i])
            dev->SetRenderTarget(i, bound.colour[i]);
    m_state->Apply();
    dev->SetDepthStencilSurface(bound.depth);
    dev->SetStreamSource(0, stream, streamOffset, streamStride);
    SafeRelease(stream);
}

bool GradingRenderer::GradeTarget(IDirect3DDevice9* dev, const BoundTargets& bound, const D3DVIEWPORT9& world,
                                  float strength)
{
    D3DSURFACE_DESC desc = {};
    if (!bound.colour[0])
        return Skip("no render target");
    if (FAILED(bound.colour[0]->GetDesc(&desc)))
        return Skip("surface description failed");
    if (!InsideTarget(world, desc))
        return Skip("world viewport outside the render target");
    if (!CopyWorld(dev, bound.colour[0], desc, world))
        return false;
    DrawGraded(dev, bound, world, strength);
    ++m_grades;
    return true;
}

bool GradingRenderer::Grade(IDirect3DDevice9* dev, const D3DVIEWPORT9& world, const float* curve, float strength)
{
    m_skip = "";
    if (dev->TestCooperativeLevel() != D3D_OK)
        return Skip("device not ready");
    if (!EnsureShaders(dev) || !EnsureStateBlock(dev) || !EnsureCurve(dev, curve))
        return false;
    BoundTargets bound;
    for (DWORD i = 0; i < kRenderTargets; ++i)
        dev->GetRenderTarget(i, &bound.colour[i]);
    dev->GetDepthStencilSurface(&bound.depth);
    const bool graded = GradeTarget(dev, bound, world, strength);
    for (IDirect3DSurface9*& colour : bound.colour)
        SafeRelease(colour);
    SafeRelease(bound.depth);
    return graded;
}
