#include "renderer.h"

#include "fog_data.h"
#include "fog_model.h"
#include "engine_shadows.h"
#include "noise_volume.h"
#include "log.h"

#include "ps_composite_low.h"
#include "ps_composite_mid.h"
#include "ps_composite_high.h"
#include "ps_march_high.h"
#include "ps_march_low.h"
#include "ps_march_mid.h"
#include "ps_probe.h"
#include "ps_ray_blur.h"
#include "ps_ray_mask.h"
#include "ps_temporal.h"
#include "ps_history_depth.h"
#include "vs_fullscreen.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace
{
constexpr UINT kPixelConstants = 99;
constexpr DWORD kStages = 10;
constexpr UINT kRayScale = 4;
constexpr float kMinViewportDepthExtent = 0.01f;
constexpr float kDeepestWorldDepthInFullRangeViewport = 0.9999995f;
constexpr float kWorldDepthMargin = 2.0e-6f;
constexpr float kSummarySeconds = 60.0f;
constexpr float kProbeSeconds = 60.0f;
constexpr float kProbeDebugSeconds = 30.0f;
constexpr unsigned kProbeFirstFrame = 60;
constexpr unsigned kInfoLevelGpuStallingProbeLimit = 5;
constexpr int kProbeGridSide = 5;
constexpr UINT kProbePoints = kProbeGridSide * kProbeGridSide;
constexpr float kRayFalloff = 8.0f;
constexpr float kRayThreshold = 0.12f;
constexpr float kRayStep = 0.075f;
constexpr float kRayDecay = 0.9f;
constexpr int kRayTaps = 8;
constexpr float kHistoryMaxSeconds = 0.25f;
constexpr float kHistoryMaxMove = 30.0f;
constexpr float kHistoryMinForwardDot = 0.70710678f;
constexpr float kShadowMinStep = 1.5f;
constexpr float kShadowStepPerYard = 0.035f;
constexpr float kShadowThicknessSteps = 4.0f;
constexpr float kLoggedStormBlendSteps = 10.0f;

const D3DRENDERSTATETYPE kRenderStates[] = {
    D3DRS_ZENABLE,          D3DRS_ZWRITEENABLE,  D3DRS_ALPHATESTENABLE,   D3DRS_ALPHABLENDENABLE,
    D3DRS_SRCBLEND,         D3DRS_DESTBLEND,     D3DRS_BLENDOP,           D3DRS_SEPARATEALPHABLENDENABLE,
    D3DRS_CULLMODE,         D3DRS_STENCILENABLE, D3DRS_TWOSIDEDSTENCILMODE, D3DRS_SCISSORTESTENABLE,
    D3DRS_COLORWRITEENABLE, D3DRS_SRGBWRITEENABLE, D3DRS_FOGENABLE,       D3DRS_CLIPPLANEENABLE,
    D3DRS_FILLMODE,
};

const D3DSAMPLERSTATETYPE kSamplerStates[] = {
    D3DSAMP_ADDRESSU, D3DSAMP_ADDRESSV,  D3DSAMP_ADDRESSW,    D3DSAMP_MAGFILTER,
    D3DSAMP_MINFILTER, D3DSAMP_MIPFILTER, D3DSAMP_SRGBTEXTURE, D3DSAMP_MAXMIPLEVEL,
};

struct Float4
{
    float x, y, z, w;
};

struct ScopedWorldShadows
{
    WorldShadowInputs inputs;

    ~ScopedWorldShadows()
    {
        engine::ReleaseWorldShadows(inputs);
    }
};

void LogShaderCaps(IDirect3DDevice9* device, LogLevel level)
{
    D3DCAPS9 caps = {};
    const HRESULT result = device->GetDeviceCaps(&caps);
    if (SUCCEEDED(result))
        LogWrite(level, "shader caps: VS 0x%08lX PS 0x%08lX PS3 slots %lu executed %lu temps %d flow %d/%d",
                 caps.VertexShaderVersion, caps.PixelShaderVersion, caps.MaxPixelShader30InstructionSlots,
                 caps.MaxPShaderInstructionsExecuted, caps.PS20Caps.NumTemps,
                 caps.PS20Caps.DynamicFlowControlDepth, caps.PS20Caps.StaticFlowControlDepth);
    else
        LogWrite(level, "shader caps unavailable: HRESULT 0x%08lX", static_cast<unsigned long>(result));
}

template <typename T>
void SafeRelease(T*& p)
{
    if (p)
    {
        p->Release();
        p = nullptr;
    }
}

long long Ticks()
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

double TickSeconds(long long ticks)
{
    static LARGE_INTEGER freq = [] {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return f;
    }();
    return static_cast<double>(ticks) / static_cast<double>(freq.QuadPart);
}

bool CreateTarget(IDirect3DDevice9* dev, UINT w, UINT h, D3DFORMAT fmt, IDirect3DTexture9** out)
{
    return SUCCEEDED(dev->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, fmt, D3DPOOL_DEFAULT, out, nullptr));
}

void SetTarget(IDirect3DDevice9* dev, IDirect3DTexture9* tex)
{
    IDirect3DSurface9* surface = nullptr;
    if (SUCCEEDED(tex->GetSurfaceLevel(0, &surface)))
    {
        dev->SetRenderTarget(0, surface);
        surface->Release();
    }
}

void Normalize3(float* v)
{
    float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (len > 1e-6f)
    {
        v[0] /= len;
        v[1] /= len;
        v[2] /= len;
    }
}

RECT ViewportRect(const D3DVIEWPORT9& vp)
{
    return {static_cast<LONG>(vp.X), static_cast<LONG>(vp.Y), static_cast<LONG>(vp.X + vp.Width),
            static_cast<LONG>(vp.Y + vp.Height)};
}

bool WorldViewFromCameraRelative(const FrameInputs& in, float* viewToWorld, float* worldToView)
{
    if (!Invert4x4(in.cameraRelativeView, viewToWorld))
        return false;
    viewToWorld[12] = in.camPos[0];
    viewToWorld[13] = in.camPos[1];
    viewToWorld[14] = in.camPos[2];
    return Invert4x4(viewToWorld, worldToView);
}

struct WorldDepthMapping
{
    float atInfinity;
    float perInverseViewDepth;
    float deepest;
};

WorldDepthMapping MapWorldDepth(const float* proj, const D3DVIEWPORT9& vp)
{
    const bool usableRange = vp.MaxZ - vp.MinZ > kMinViewportDepthExtent && vp.MinZ >= 0.0f && vp.MaxZ <= 1.0f;
    const float worldMinZ = usableRange ? vp.MinZ : 0.0f;
    const float worldExtent = usableRange ? vp.MaxZ - vp.MinZ : 1.0f;
    const float worldMaxZ = worldMinZ + worldExtent;
    return {worldMinZ + worldExtent * (1.0f + proj[10]) * 0.5f, worldExtent * proj[14] * 0.5f,
            worldMaxZ >= kDeepestWorldDepthInFullRangeViewport ? kDeepestWorldDepthInFullRangeViewport
                                                               : worldMaxZ + kWorldDepthMargin};
}

enum class FogBlend
{
    GammaFixedFunction,
    LinearOverSceneCopy,
    LinearFixedFunction,
    GammaOverSceneCopy,
};

const char* FogBlendName(FogBlend blend)
{
    static const char* const kNames[] = {"gamma, fixed function", "linear over a scene copy",
                                         "linear, fixed function (no scene copy)", "gamma over a scene copy"};
    return kNames[static_cast<int>(blend)];
}

struct ProbeSample
{
    float rawDepth;
    float viewDepthYards;
    float fogOpacity;
    float depthClass;
};
static_assert(sizeof(ProbeSample) == sizeof(Float4), "ProbeSample is one A32B32G32R32F probe texel");

char DepthClassLetter(float depthClass)
{
    return depthClass > 1.5f ? 's' : (depthClass > 0.5f ? 'f' : 'w');
}
}

Renderer::~Renderer()
{
    ReleaseAll();
}

void Renderer::ReleaseDefaultPool()
{
    SafeRelease(m_marchTarget);
    SafeRelease(m_history[0]);
    SafeRelease(m_history[1]);
    SafeRelease(m_historyDepth);
    SafeRelease(m_rays[0]);
    SafeRelease(m_rays[1]);
    SafeRelease(m_sceneCopy);
    SafeRelease(m_localLightData);
    SafeRelease(m_densityNoise);
    SafeRelease(m_probeTarget);
    SafeRelease(m_probeReadback);
    SafeRelease(m_state);
    m_lowW = m_lowH = m_rayW = m_rayH = 0;
    m_sceneCopyW = m_sceneCopyH = 0;
    m_sceneCopyFailed = false;
    m_probeFailed = false;
    m_historyValid = false;
    m_adaptiveLightingHistory = false;
    m_prevLocalLightCount = 0;
}

void Renderer::ReleaseAll()
{
    ReleaseDefaultPool();
    m_unsupportedShaderDevice = nullptr;
    SafeRelease(m_vs);
    for (auto*& ps : m_march)
        SafeRelease(ps);
    SafeRelease(m_temporal);
    SafeRelease(m_historyDepthShader);
    for (auto*& ps : m_composite)
        SafeRelease(ps);
    SafeRelease(m_rayMask);
    SafeRelease(m_rayBlur);
    SafeRelease(m_probe);
    SafeRelease(m_decl);
}

void Renderer::LogLightChange(const FrameInputs& in, const AuthoredFog& fog, bool authored)
{
    const LightParamsSelection& selection = in.lightParams;
    const auto loggedStormStep = static_cast<uint32_t>(std::lround(selection.stormBlend * kLoggedStormBlendSteps));
    uint32_t signature = authored ? 0x80000000u : 0u;
    for (int i = 0; i < fog.lightCount; ++i)
        signature = signature * 31u + fog.lightIds[i];
    signature = signature * 31u + loggedStormStep;
    signature = signature * 31u + static_cast<uint32_t>(selection.screenEffectSlot);
    signature ^= static_cast<uint32_t>(in.mapId) << 20;
    if (m_lightsLogged && signature == m_lightSignature)
        return;
    m_lightsLogged = true;
    m_lightSignature = signature;
    if (!authored)
    {
        VF_LOG_INFO("map %d at (%.0f %.0f %.0f): no Classic fog data, derived layers", in.mapId, in.camPos[0],
                    in.camPos[1], in.camPos[2]);
        return;
    }
    char lights[160] = {};
    int used = 0;
    for (int i = 0; i < fog.lightCount && used < static_cast<int>(sizeof(lights)) - 24; ++i)
        used += std::snprintf(lights + used, sizeof(lights) - used, "%s%u:%.2f", i ? " " : "", fog.lightIds[i],
                              fog.lightWeights[i]);
    VF_LOG_INFO("map %d at (%.0f %.0f %.0f): Classic lights %s, %d layers, storm %.1f, screen effect slot %d",
                in.mapId, in.camPos[0], in.camPos[1], in.camPos[2], lights, fog.layerCount,
                loggedStormStep / kLoggedStormBlendSteps, selection.screenEffectSlot);
}

bool Renderer::Skip(const char* reason)
{
    m_skip = reason;
    m_historyValid = false;
    return false;
}

bool Renderer::EnsureShaders(IDirect3DDevice9* dev)
{
    if (m_unsupportedShaderDevice == dev)
        return Skip("required shader unsupported");
    if (m_vs)
        return true;
    struct PixelShaderRequest
    {
        const char* name;
        const BYTE* code;
        IDirect3DPixelShader9** output;
    };
    const PixelShaderRequest pixels[] = {
        {"ps_march_low", g_ps_march_low, &m_march[0]},
        {"ps_march_mid", g_ps_march_mid, &m_march[1]},
        {"ps_march_high", g_ps_march_high, &m_march[2]},
        {"ps_temporal", g_ps_temporal, &m_temporal},
        {"ps_history_depth", g_ps_history_depth, &m_historyDepthShader},
        {"ps_composite_low", g_ps_composite_low, &m_composite[0]},
        {"ps_composite_mid", g_ps_composite_mid, &m_composite[1]},
        {"ps_composite_high", g_ps_composite_high, &m_composite[2]},
        {"ps_ray_mask", g_ps_ray_mask, &m_rayMask},
        {"ps_ray_blur", g_ps_ray_blur, &m_rayBlur},
    };
    static const D3DVERTEXELEMENT9 kElements[] = {
        {0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        D3DDECL_END(),
    };
    const char* failedName = "vs_fullscreen";
    HRESULT result = dev->CreateVertexShader(reinterpret_cast<const DWORD*>(g_vs_fullscreen), &m_vs);
    if (SUCCEEDED(result))
        for (const PixelShaderRequest& shader : pixels)
        {
            failedName = shader.name;
            result = dev->CreatePixelShader(reinterpret_cast<const DWORD*>(shader.code), shader.output);
            if (FAILED(result))
                break;
        }
    if (SUCCEEDED(result))
    {
        failedName = "fullscreen vertex declaration";
        result = dev->CreateVertexDeclaration(kElements, &m_decl);
    }
    if (FAILED(result))
    {
        const bool unsupported = (result == D3DERR_INVALIDCALL || result == D3DERR_NOTAVAILABLE ||
                                  result == E_INVALIDARG) && dev->TestCooperativeLevel() == D3D_OK;
        VF_LOG_ERROR("shader initialization failed: %s HRESULT 0x%08lX; %s", failedName,
                     static_cast<unsigned long>(result), unsupported ? "unsupported on this device" : "will retry");
        LogShaderCaps(dev, LogLevel::Error);
        ReleaseAll();
        if (unsupported)
            m_unsupportedShaderDevice = dev;
        return Skip(unsupported ? "required shader unsupported" : "shader creation failed");
    }
    LogShaderCaps(dev, LogLevel::Info);
    return true;
}

bool Renderer::EnsureStateBlock(IDirect3DDevice9* dev)
{
    if (m_state)
        return true;
    if (FAILED(dev->BeginStateBlock()))
        return Skip("state block recording failed");
    for (D3DRENDERSTATETYPE rs : kRenderStates)
        dev->SetRenderState(rs, 0);
    for (DWORD stage = 0; stage < kStages; ++stage)
    {
        dev->SetTexture(stage, nullptr);
        for (D3DSAMPLERSTATETYPE ss : kSamplerStates)
            dev->SetSamplerState(stage, ss, 0);
    }
    float zeros[kPixelConstants * 4] = {};
    dev->SetVertexShader(nullptr);
    dev->SetPixelShader(nullptr);
    dev->SetPixelShaderConstantF(0, zeros, kPixelConstants);
    dev->SetVertexDeclaration(m_decl);
    dev->SetStreamSource(0, nullptr, 0, 0);
    dev->SetStreamSourceFreq(0, 1);
    D3DVIEWPORT9 vp = {0, 0, 1, 1, 0.0f, 1.0f};
    dev->SetViewport(&vp);
    RECT scissor = {0, 0, 1, 1};
    dev->SetScissorRect(&scissor);
    if (FAILED(dev->EndStateBlock(&m_state)) || !m_state)
    {
        m_state = nullptr;
        return Skip("state block recording failed");
    }
    return true;
}

bool Renderer::EnsureTargets(IDirect3DDevice9* dev, UINT lowW, UINT lowH, UINT rayW, UINT rayH)
{
    if (m_marchTarget && m_lowW == lowW && m_lowH == lowH && m_rayW == rayW && m_rayH == rayH)
        return true;
    SafeRelease(m_marchTarget);
    SafeRelease(m_history[0]);
    SafeRelease(m_history[1]);
    SafeRelease(m_historyDepth);
    SafeRelease(m_rays[0]);
    SafeRelease(m_rays[1]);
    m_historyValid = false;

    D3DFORMAT fogFormat = D3DFMT_A16B16G16R16F;
    bool ok = CreateTarget(dev, lowW, lowH, fogFormat, &m_marchTarget);
    if (!ok)
    {
        fogFormat = D3DFMT_A8R8G8B8;
        ok = CreateTarget(dev, lowW, lowH, fogFormat, &m_marchTarget);
    }
    ok = ok && CreateTarget(dev, lowW, lowH, fogFormat, &m_history[0]) &&
         CreateTarget(dev, lowW, lowH, fogFormat, &m_history[1]) &&
         CreateTarget(dev, lowW, lowH, D3DFMT_A8R8G8B8, &m_historyDepth) &&
         CreateTarget(dev, rayW, rayH, D3DFMT_A8R8G8B8, &m_rays[0]) &&
         CreateTarget(dev, rayW, rayH, D3DFMT_A8R8G8B8, &m_rays[1]);
    if (!ok)
    {
        VF_LOG_ERROR("render target creation failed (%ux%u)", lowW, lowH);
        SafeRelease(m_marchTarget);
        SafeRelease(m_history[0]);
        SafeRelease(m_history[1]);
        SafeRelease(m_historyDepth);
        SafeRelease(m_rays[0]);
        SafeRelease(m_rays[1]);
        return Skip("render target creation failed");
    }

    m_lowW = lowW;
    m_lowH = lowH;
    m_rayW = rayW;
    m_rayH = rayH;
    VF_LOG_INFO("targets: fog %ux%u (%s), depth history rgba8, rays %ux%u", lowW, lowH,
                fogFormat == D3DFMT_A16B16G16R16F ? "fp16" : "rgba8", rayW, rayH);
    return true;
}

bool Renderer::EnsureSceneCopy(IDirect3DDevice9* dev, IDirect3DSurface9* target, UINT w, UINT h)
{
    if (m_sceneCopy && m_sceneCopyW == w && m_sceneCopyH == h)
        return true;
    SafeRelease(m_sceneCopy);
    m_sceneCopyW = m_sceneCopyH = 0;
    if (m_sceneCopyFailed)
        return false;
    D3DSURFACE_DESC desc = {};
    target->GetDesc(&desc);
    if (!CreateTarget(dev, w, h, desc.Format, &m_sceneCopy) &&
        !CreateTarget(dev, w, h, D3DFMT_A8R8G8B8, &m_sceneCopy))
    {
        m_sceneCopyFailed = true;
        VF_LOG_ERROR("scene copy creation failed (%ux%u); fog blends in gamma space", w, h);
        return false;
    }
    m_sceneCopyW = w;
    m_sceneCopyH = h;
    return true;
}

bool Renderer::CopyWorldViewport(IDirect3DDevice9* dev, IDirect3DSurface9* target, const D3DVIEWPORT9& vp)
{
    if (!EnsureSceneCopy(dev, target, vp.Width, vp.Height))
        return false;
    const RECT world = ViewportRect(vp);
    IDirect3DSurface9* sceneSurface = nullptr;
    const bool copied = SUCCEEDED(m_sceneCopy->GetSurfaceLevel(0, &sceneSurface)) &&
                        SUCCEEDED(dev->StretchRect(target, &world, sceneSurface, nullptr, D3DTEXF_POINT));
    SafeRelease(sceneSurface);
    return copied;
}

bool Renderer::DepthProbeDue(long long now) const
{
    const bool debugLog = LogEnabled(LogLevel::Debug);
    const float probeIntervalSeconds = debugLog ? kProbeDebugSeconds : kProbeSeconds;
    return LogEnabled(LogLevel::Info) && !m_probeFailed && m_frame >= kProbeFirstFrame &&
           (debugLog || m_probeAttempts < kInfoLevelGpuStallingProbeLimit) &&
           (m_probeAttempts == 0 || TickSeconds(now - m_probeTicks) > probeIntervalSeconds);
}

void Renderer::LogDepthProbe(IDirect3DDevice9* dev, IDirect3DTexture9* depthTexture, IDirect3DTexture9* fog,
                             const D3DVIEWPORT9& vp, float deepestWorldDepth, float dayFraction)
{
    if (m_probeFailed)
        return;
    if ((!m_probe && FAILED(dev->CreatePixelShader(reinterpret_cast<const DWORD*>(g_ps_probe), &m_probe))) ||
        (!m_probeTarget &&
         (!CreateTarget(dev, kProbePoints, 1, D3DFMT_A32B32G32R32F, &m_probeTarget) ||
          FAILED(dev->CreateOffscreenPlainSurface(kProbePoints, 1, D3DFMT_A32B32G32R32F, D3DPOOL_SYSTEMMEM,
                                                  &m_probeReadback, nullptr)))))
    {
        SafeRelease(m_probeTarget);
        SafeRelease(m_probeReadback);
        m_probeFailed = true;
        VF_LOG_INFO("depth probe unavailable (no probe shader or float render target)");
        return;
    }
    SetTarget(dev, m_probeTarget);
    D3DVIEWPORT9 probeVp = {0, 0, kProbePoints, 1, 0.0f, 1.0f};
    dev->SetViewport(&probeVp);
    dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
    dev->SetPixelShader(m_probe);
    BindTexture(dev, 0, depthTexture, false);
    BindTexture(dev, 1, fog, false);
    DrawFullscreen(dev);

    IDirect3DSurface9* surface = nullptr;
    D3DLOCKED_RECT locked = {};
    bool read = SUCCEEDED(m_probeTarget->GetSurfaceLevel(0, &surface)) &&
                SUCCEEDED(dev->GetRenderTargetData(surface, m_probeReadback)) &&
                SUCCEEDED(m_probeReadback->LockRect(&locked, nullptr, D3DLOCK_READONLY));
    SafeRelease(surface);
    if (!read)
    {
        VF_LOG_INFO("depth probe %u: readback failed", m_probeAttempts);
        return;
    }
    const auto* samples = static_cast<const ProbeSample*>(locked.pBits);
    VF_LOG_INFO("depth probe %u: day %.4f, viewport depth %.4f..%.4f, world depth up to %.7f; per point raw depth / yd "
                "/ fog opacity (w world, f beyond the far clip, s sky)",
                m_probeAttempts, dayFraction, vp.MinZ, vp.MaxZ, deepestWorldDepth);
    for (int row = 0; row < kProbeGridSide; ++row)
    {
        char line[256] = {};
        int used = 0;
        for (int col = 0; col < kProbeGridSide; ++col)
        {
            const ProbeSample& sample = samples[row * kProbeGridSide + col];
            used += std::snprintf(line + used, sizeof(line) - used, "  %.7f/%.0f/%.2f %c", sample.rawDepth,
                                  sample.viewDepthYards, sample.fogOpacity, DepthClassLetter(sample.depthClass));
        }
        VF_LOG_INFO("  row %d:%s", row, line);
    }
    m_probeReadback->UnlockRect();
}

void Renderer::DrawFullscreen(IDirect3DDevice9* dev)
{
    static const float kTriangle[3][4] = {
        {-1.0f, -1.0f, 0.0f, 1.0f}, {-1.0f, 3.0f, 0.0f, 1.0f}, {3.0f, -1.0f, 0.0f, 1.0f}};
    dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, kTriangle, sizeof(kTriangle[0]));
}

void Renderer::BindTexture(IDirect3DDevice9* dev, DWORD stage, IDirect3DBaseTexture9* tex, bool linear)
{
    DWORD filter = linear ? D3DTEXF_LINEAR : D3DTEXF_POINT;
    dev->SetTexture(stage, tex);
    dev->SetSamplerState(stage, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    dev->SetSamplerState(stage, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    dev->SetSamplerState(stage, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);
    dev->SetSamplerState(stage, D3DSAMP_MAGFILTER, filter);
    dev->SetSamplerState(stage, D3DSAMP_MINFILTER, filter);
    dev->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    dev->SetSamplerState(stage, D3DSAMP_SRGBTEXTURE, FALSE);
    dev->SetSamplerState(stage, D3DSAMP_MAXMIPLEVEL, 0);
}

bool Renderer::Render(IDirect3DDevice9* dev, IDirect3DTexture9* depthTexture, IDirect3DSurface9* boundDepthStencil,
                      const FrameInputs& in, const Config& cfg)
{
    m_skip = "";
    if (dev->TestCooperativeLevel() != D3D_OK)
        return Skip("device not ready");
    if (!EnsureShaders(dev) || !EnsureStateBlock(dev))
        return false;

    IDirect3DSurface9* saved[4] = {};
    for (DWORD i = 0; i < 4; ++i)
        dev->GetRenderTarget(i, &saved[i]);
    IDirect3DSurface9* savedDepth = nullptr;
    dev->GetDepthStencilSurface(&savedDepth);

    bool ok = false;
    D3DSURFACE_DESC rtDesc = {};
    D3DSURFACE_DESC depthDesc = {};
    if (!saved[0])
        Skip("no render target");
    else if (savedDepth != boundDepthStencil)
        Skip("fog depth surface not bound");
    else if (FAILED(saved[0]->GetDesc(&rtDesc)) || FAILED(boundDepthStencil->GetDesc(&depthDesc)))
        Skip("surface description failed");
    else if (rtDesc.Width != depthDesc.Width || rtDesc.Height != depthDesc.Height)
        Skip("render target and depth sizes differ");
    else if (rtDesc.MultiSampleType != D3DMULTISAMPLE_NONE)
        Skip("multisampled render target");
    else
    {
        IDirect3DVertexBuffer9* stream = nullptr;
        UINT streamOffset = 0;
        UINT streamStride = 0;
        dev->GetStreamSource(0, &stream, &streamOffset, &streamStride);
        m_state->Capture();
        for (DWORD i = 1; i < 4; ++i)
            if (saved[i])
                dev->SetRenderTarget(i, nullptr);
        ok = RenderPasses(dev, depthTexture, saved[0], depthDesc, in, cfg);
        dev->SetRenderTarget(0, saved[0]);
        for (DWORD i = 1; i < 4; ++i)
            if (saved[i])
                dev->SetRenderTarget(i, saved[i]);
        m_state->Apply();
        dev->SetDepthStencilSurface(savedDepth);
        dev->SetStreamSource(0, stream, streamOffset, streamStride);
        SafeRelease(stream);
    }

    for (auto*& s : saved)
        SafeRelease(s);
    SafeRelease(savedDepth);
    return ok;
}

bool Renderer::RenderPasses(IDirect3DDevice9* dev, IDirect3DTexture9* depthTexture, IDirect3DSurface9* target,
                            const D3DSURFACE_DESC& depthDesc, const FrameInputs& in, const Config& cfg)
{
    const D3DVIEWPORT9 vp = in.viewport;
    if (vp.Width < 16 || vp.Height < 16 || vp.X + vp.Width > depthDesc.Width || vp.Y + vp.Height > depthDesc.Height)
        return Skip("world viewport outside the render target");

    const UINT scale = cfg.quality == 1 ? 4u : 2u;
    const UINT lowW = (vp.Width + scale - 1) / scale;
    const UINT lowH = (vp.Height + scale - 1) / scale;
    const UINT rayW = std::max(1u, static_cast<UINT>(vp.Width) / kRayScale);
    const UINT rayH = std::max(1u, static_cast<UINT>(vp.Height) / kRayScale);
    if (!EnsureTargets(dev, lowW, lowH, rayW, rayH))
        return false;

    float viewToWorld[16];
    float worldToView[16];
    if (!WorldViewFromCameraRelative(in, viewToWorld, worldToView))
        return Skip("view matrix not invertible");

    AuthoredFog authored = {};
    const bool hasAuthored =
        cfg.dataMode == 1 && GlobalFogData().Resolve(in.mapId, in.camPos, in.dayFraction, in.lightParams, authored);
    const FogParams fog = BuildFogParams(in, cfg, hasAuthored ? &authored : nullptr);
    LogLightChange(in, authored, hasAuthored);
    const float* proj = in.glProjection;
    const WorldDepthMapping worldDepth = MapWorldDepth(proj, vp);

    float toLightInView[3];
    TransformDirection(in.toLight, in.cameraRelativeView, toLightInView);
    Normalize3(toLightInView);

    ScopedWorldShadows worldShadows;
    if (cfg.lightShafts && cfg.worldShadows)
        engine::AcquireWorldShadows(dev, worldShadows.inputs);
    float shadowLightInView[3];
    TransformDirection(worldShadows.inputs.count > 0 ? worldShadows.inputs.toLight : in.toLight,
                       in.cameraRelativeView, shadowLightInView);
    Normalize3(shadowLightInView);

    float common[9][4] = {
        {static_cast<float>(vp.X), static_cast<float>(vp.Y), static_cast<float>(vp.Width),
         static_cast<float>(vp.Height)},
        {static_cast<float>(scale), static_cast<float>(m_frame % 1024u), 1.0f / depthDesc.Width,
         1.0f / depthDesc.Height},
        {proj[0], proj[5], proj[8], proj[9]},
        {worldDepth.atInfinity, worldDepth.perInverseViewDepth, fog.maxDistance, worldDepth.deepest},
        {},
        {},
        {},
        {},
        {static_cast<float>(lowW), static_cast<float>(lowH), 1.0f / lowW, 1.0f / lowH},
    };
    std::memcpy(common[4], viewToWorld, sizeof(viewToWorld));

    const long long now = Ticks();
    const float dx = in.camPos[0] - m_prevCam[0];
    const float dy = in.camPos[1] - m_prevCam[1];
    const float dz = in.camPos[2] - m_prevCam[2];
    const float forwardDot = worldToView[2] * m_prevWorldToView[2] +
                             worldToView[6] * m_prevWorldToView[6] + worldToView[10] * m_prevWorldToView[10];
    const int shadowMode = worldShadows.inputs.count | (worldShadows.inputs.hardwareComparison ? 8 : 0);
    const bool historyValid = m_historyValid && cfg.temporal > 0.0f && m_prevScale == scale &&
                              SameLiveSettings(cfg, m_prevConfig) && in.mapId == m_prevMap &&
                              shadowMode == m_prevShadowMode &&
                              in.lightParams.screenEffectSlot == m_prevLightSlot &&
                              forwardDot > kHistoryMinForwardDot &&
                              std::memcmp(m_prevProj, proj, sizeof(m_prevProj)) == 0 &&
                              std::memcmp(&m_prevViewport, &vp, sizeof(vp)) == 0 &&
                              TickSeconds(now - m_prevTicks) < kHistoryMaxSeconds &&
                              dx * dx + dy * dy + dz * dz < kHistoryMaxMove * kHistoryMaxMove;
    float reproj[16];
    float prevViewProj[16];
    Mul4x4(m_prevWorldToView, m_prevProj, prevViewProj);
    Mul4x4(viewToWorld, prevViewProj, reproj);

    float sunPx[2] = {};
    bool sunInFront = toLightInView[2] > 0.05f;
    float sunScreenFade = 0.0f;
    if (sunInFront)
    {
        float ndcX = toLightInView[0] / toLightInView[2] * proj[0] + proj[8];
        float ndcY = toLightInView[1] / toLightInView[2] * proj[5] + proj[9];
        sunPx[0] = vp.X + (ndcX * 0.5f + 0.5f) * vp.Width;
        sunPx[1] = vp.Y + (0.5f - ndcY * 0.5f) * vp.Height;
        sunScreenFade = std::clamp(1.6f - std::max(std::fabs(ndcX), std::fabs(ndcY)), 0.0f, 1.0f);
    }
    const float rayStrength = cfg.godRays * fog.lightVisibility * sunScreenFade;
    const bool rays = rayStrength > 0.005f;

    const bool viewChanged = std::fabs(in.farClip - m_loggedFarClip) > 1.0f || vp.Width != m_loggedViewport.Width ||
                             vp.Height != m_loggedViewport.Height || vp.MinZ != m_loggedViewport.MinZ ||
                             vp.MaxZ != m_loggedViewport.MaxZ;
    if (m_logged < 1 || viewChanged || TickSeconds(now - m_summaryTicks) > kSummarySeconds ||
        (LogEnabled(LogLevel::Debug) && m_frame % 600u == 0))
    {
        ++m_logged;
        m_summaryTicks = now;
        m_loggedFarClip = in.farClip;
        m_loggedViewport = vp;
        float projectionNear = -proj[14] / (1.0f + proj[10]);
        float projectionFar = proj[14] / (1.0f - proj[10]);
        VF_LOG_INFO("frame %u: viewport %lu,%lu %lux%lu of %ux%u depth %.4f..%.4f; near %.3f far %.1f (farclip %.1f) "
                    "maxdist %.0f",
                    m_frame, vp.X, vp.Y, vp.Width, vp.Height, depthDesc.Width, depthDesc.Height, vp.MinZ, vp.MaxZ,
                    projectionNear, projectionFar, in.farClip, fog.maxDistance);
        VF_LOG_INFO("  camera (%.2f %.2f %.2f) inverse-view origin (%.2f %.2f %.2f) target (%.2f %.2f %.2f)",
                    in.camPos[0], in.camPos[1], in.camPos[2], viewToWorld[12], viewToWorld[13], viewToWorld[14],
                    in.camTarget[0], in.camTarget[1], in.camTarget[2]);
        VF_LOG_INFO("  day %.4f %s toLight (%.3f %.3f %.3f) view (%.3f %.3f %.3f) vis %.2f above %.2f "
                    "sunPx (%.0f %.0f) rays %.2f",
                    in.dayFraction, in.lightIsMoon ? "moon" : "sun", in.toLight[0], in.toLight[1], in.toLight[2],
                    toLightInView[0], toLightInView[1], toLightInView[2], fog.lightVisibility, fog.lightAboveHorizon,
                    sunPx[0], sunPx[1], rayStrength);
        VF_LOG_INFO("  map %d fog %08X start %.1f end %.1f zone %.1f sun %08X direct %08X ambient %08X refZ %.1f "
                    "glow %.2f, direct light vs Classic %.2f",
                    in.mapId, in.fogColor, in.fogStart, in.fogEnd, in.zoneFogDistance, in.sunColor, in.directColor,
                    in.ambientColor, fog.referenceZ, in.clientGlowAmount, fog.directLightMatch);
        VF_LOG_INFO("  local points %u enabled %d; interior %d blend %.3f; world shadow maps %d; light shafts %d",
                    in.localLights.pointLightCount, cfg.localLights, in.localLights.cameraInterior,
                    in.localLights.interiorBlend, worldShadows.inputs.count, cfg.lightShafts);
        for (int i = 0; i < kFogLayers; ++i)
        {
            const FogLayer& l = fog.layers[i];
            VF_LOG_INFO("  layer %d (%s): start %.0f density %.6f curve %.2f^%.2f g %.2f diffuse %.2f %.2f %.2f "
                        "emissive %.2f %.2f %.2f upper %.1f/%.4f lower %.1f/%.4f shadowed %.0f limit %.0f",
                        i, fog.authored && i < kSceneLayers ? "classic" : "derived", l.start, l.density, l.strength,
                        l.exponent, l.g, l.diffuse[0], l.diffuse[1], l.diffuse[2], l.emissive[0], l.emissive[1],
                        l.emissive[2], l.upperHeight, l.upperFalloff, l.lowerHeight, l.lowerFalloff, l.shadowed,
                        std::min(l.endDistance, 99999.0f));
        }
    }

    dev->SetDepthStencilSurface(nullptr);
    dev->SetVertexShader(m_vs);
    dev->SetVertexDeclaration(m_decl);
    dev->SetStreamSourceFreq(0, 1);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    dev->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    dev->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE, FALSE);
    dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
    dev->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
    dev->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
    dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    dev->SetPixelShaderConstantF(0, &common[0][0], 9);

    SetTarget(dev, m_marchTarget);
    dev->SetPixelShader(m_march[std::clamp(cfg.quality, 1, 3) - 1]);
    const Float4 march[3] = {
        {shadowLightInView[0], shadowLightInView[1], shadowLightInView[2], fog.shadowedLayerLightScale},
        {kShadowMinStep, kShadowStepPerYard,
         cfg.lightShafts && (fog.authored ? fog.lightAboveHorizon : fog.lightVisibility) > 0.001f ? 1.0f : 0.0f,
         kShadowThicknessSteps},
        {cfg.temporal > 0.0f ? 1.0f : 0.0f, fog.maxDistance, fog.horizonStart, fog.farClip},
    };
    dev->SetPixelShaderConstantF(9, &march[0].x, 3);
    static_assert(sizeof(FogLayer) == 6 * sizeof(Float4), "FogLayer is six shader registers");
    dev->SetPixelShaderConstantF(12, &fog.layers[0].start, 6 * kFogLayers);
    const WorldShadowInputs& shadows = worldShadows.inputs;
    const Float4 shadowControl = {static_cast<float>(shadows.count), shadows.hardwareComparison ? 1.0f : 0.0f,
                                  0.0f, 0.0f};
    dev->SetPixelShaderConstantF(36, &shadowControl.x, 1);
    dev->SetPixelShaderConstantF(37, &shadows.viewToShadow[0][0], 12);
    Float4 shadowTexels[kWorldShadowMapCount] = {};
    for (DWORD i = 0; i < kWorldShadowMapCount; ++i)
    {
        shadowTexels[i] = {shadows.texelSize[i][0], shadows.texelSize[i][1], 0.0f, 0.0f};
        BindTexture(dev, 4 + i, shadows.textures[i], shadows.hardwareComparison);
    }
    dev->SetPixelShaderConstantF(49, &shadowTexels[0].x, kWorldShadowMapCount);
    uint32_t pointLightCount = cfg.localLights ? std::min(in.localLights.pointLightCount, kMaxLocalPointLights) : 0;
    Float4 localConstants[32] = {};
    for (uint32_t i = 0; i < pointLightCount; ++i)
    {
        const LocalPointLight& light = in.localLights.pointLights[i];
        float relative[3] = {light.position[0] - in.camPos[0], light.position[1] - in.camPos[1],
                              light.position[2] - in.camPos[2]};
        float position[3];
        TransformDirection(relative, in.cameraRelativeView, position);
        localConstants[i * 3] = {position[0], position[1], position[2], light.cutoff};
        float color[3];
        for (int channel = 0; channel < 3; ++channel)
            color[channel] = (fog.linear ? std::pow(std::max(light.color[channel], 0.0f), 2.2f)
                                         : light.color[channel]) * cfg.localLightIntensity;
        localConstants[i * 3 + 1] = {color[0], color[1], color[2], 0.0f};
        localConstants[i * 3 + 2] = {light.attenuation[0], light.attenuation[1], light.attenuation[2], 0.0f};
    }
    if (pointLightCount > 0)
    {
        if (!m_localLightData)
            dev->CreateTexture(32, 1, 1, 0, D3DFMT_A32B32G32R32F, D3DPOOL_MANAGED, &m_localLightData, nullptr);
        D3DLOCKED_RECT locked = {};
        if (m_localLightData && SUCCEEDED(m_localLightData->LockRect(0, &locked, nullptr, 0)))
        {
            std::memcpy(locked.pBits, localConstants, sizeof(localConstants));
            m_localLightData->UnlockRect(0);
        }
        else
            pointLightCount = 0;
    }
    float marchLightLimit = 0.0f;
    for (uint32_t i = 0; i < pointLightCount; ++i)
    {
        const Float4& positionRadius = localConstants[i * 3];
        const double x = positionRadius.x;
        const double y = positionRadius.y;
        const double z = positionRadius.z;
        const float limit = static_cast<float>(std::sqrt(x * x + y * y + z * z) + positionRadius.w + 0.001);
        marchLightLimit = std::max(marchLightLimit, std::nextafter(limit, std::numeric_limits<float>::infinity()));
    }
    const Float4 localControl = {static_cast<float>(pointLightCount), marchLightLimit, 0.0f, 0.0f};
    dev->SetPixelShaderConstantF(53, &localControl.x, 1);
    BindTexture(dev, 8, m_localLightData, false);
    if (cfg.noiseAmount > 0.0f && !m_densityNoise)
        CreateDensityNoise(dev, &m_densityNoise);
    BindTexture(dev, 9, m_densityNoise, true);
    dev->SetSamplerState(9, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    dev->SetSamplerState(9, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
    dev->SetSamplerState(9, D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP);
    const double windPeriod = static_cast<double>(kDensityNoiseSize) / std::max(cfg.noiseScale, 0.001f);
    const Float4 variation = {m_densityNoise ? cfg.noiseAmount : 0.0f, cfg.noiseScale,
                              static_cast<float>(std::fmod(TickSeconds(now) * cfg.noiseWindSpeed, windPeriod)), 0.0f};
    dev->SetPixelShaderConstantF(78, &variation.x, 1);
    BindTexture(dev, 0, depthTexture, false);
    DrawFullscreen(dev);

    const int write = m_historyIndex ^ 1;
    SetTarget(dev, m_history[write]);
    dev->SetPixelShader(m_temporal);
    m_adaptiveLightingHistory = pointLightCount > 0 || m_prevLocalLightCount > 0 ||
                                shadows.count > 0 || cfg.noiseAmount > 0.0f;
    const Float4 temporal = {cfg.temporal, historyValid ? 1.0f : 0.0f,
                             m_adaptiveLightingHistory ? 1.0f : 0.0f, 0.0f};
    dev->SetPixelShaderConstantF(9, reproj, 4);
    dev->SetPixelShaderConstantF(13, &temporal.x, 1);
    BindTexture(dev, 0, m_marchTarget, false);
    BindTexture(dev, 1, m_history[m_historyIndex], false);
    BindTexture(dev, 2, depthTexture, false);
    BindTexture(dev, 3, m_historyDepth, false);
    DrawFullscreen(dev);

    BindTexture(dev, 3, nullptr, false);
    SetTarget(dev, m_historyDepth);
    dev->SetPixelShader(m_historyDepthShader);
    BindTexture(dev, 0, depthTexture, false);
    DrawFullscreen(dev);

    if (rays)
    {
        IDirect3DSurface9* raySurface = nullptr;
        const RECT world = ViewportRect(vp);
        if (SUCCEEDED(m_rays[0]->GetSurfaceLevel(0, &raySurface)))
        {
            dev->StretchRect(target, &world, raySurface, nullptr, D3DTEXF_LINEAR);
            raySurface->Release();
        }
        SetTarget(dev, m_rays[1]);
        dev->SetPixelShader(m_rayMask);
        const Float4 mask[2] = {
            {toLightInView[0], toLightInView[1], toLightInView[2], kRayFalloff},
            {0.0f, kRayThreshold, 1.0f / rayW, 1.0f / rayH},
        };
        dev->SetPixelShaderConstantF(9, &mask[0].x, 2);
        BindTexture(dev, 0, depthTexture, false);
        BindTexture(dev, 1, m_rays[0], true);
        DrawFullscreen(dev);

        float norm = 0.0f;
        for (int k = 0; k < kRayTaps; ++k)
            norm += std::pow(kRayDecay, static_cast<float>(k));
        const float sunUv[2] = {(sunPx[0] - vp.X) / vp.Width, (sunPx[1] - vp.Y) / vp.Height};
        dev->SetPixelShader(m_rayBlur);
        float step = kRayStep;
        for (int pass = 0; pass < 2; ++pass)
        {
            SetTarget(dev, m_rays[pass == 0 ? 0 : 1]);
            const Float4 blur[2] = {
                {sunUv[0], sunUv[1], step, 1.0f / norm},
                {static_cast<float>(rayW), static_cast<float>(rayH), 1.0f / rayW, 1.0f / rayH},
            };
            dev->SetPixelShaderConstantF(9, &blur[0].x, 2);
            BindTexture(dev, 0, m_rays[pass == 0 ? 1 : 0], true);
            DrawFullscreen(dev);
            step /= kRayTaps;
        }
    }

    FogBlend blend = FogBlend::GammaFixedFunction;
    if (fog.linear)
        blend = CopyWorldViewport(dev, target, vp) ? FogBlend::LinearOverSceneCopy : FogBlend::LinearFixedFunction;
    else if (rays && CopyWorldViewport(dev, target, vp))
        blend = FogBlend::GammaOverSceneCopy;
    const bool sceneBlend = blend == FogBlend::LinearOverSceneCopy || blend == FogBlend::GammaOverSceneCopy;
    const float blendMode = static_cast<float>(blend);
    if (blendMode != m_loggedBlendMode)
    {
        m_loggedBlendMode = blendMode;
        VF_LOG_INFO("fog blend: %s", FogBlendName(blend));
    }

    const RECT world = ViewportRect(vp);
    dev->SetRenderTarget(0, target);
    dev->SetViewport(&vp);
    dev->SetScissorRect(&world);
    dev->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, sceneBlend ? FALSE : TRUE);
    dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
    dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    dev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    dev->SetRenderState(D3DRS_COLORWRITEENABLE,
                        D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
    dev->SetPixelShader(m_composite[cfg.quality - 1]);
    dev->SetPixelShaderConstantF(9, &march[0].x, 3);
    dev->SetPixelShaderConstantF(12, &fog.layers[0].start, 6 * kFogLayers);
    const Float4 composite[3] = {
        {fog.authored ? cfg.classicExposure : cfg.exposure, rays && sceneBlend ? rayStrength : 0.0f,
         static_cast<float>(cfg.debugView), blendMode},
        {fog.rayColor[0], fog.rayColor[1], fog.rayColor[2], 0.0f},
        {sunPx[0], sunPx[1], cfg.sunMarker && sunInFront ? 1.0f : 0.0f,
         cfg.glowCompensation && sceneBlend ? in.clientGlowAmount : 0.0f},
    };
    dev->SetPixelShaderConstantF(96, &composite[0].x, 3);
    BindTexture(dev, 0, depthTexture, false);
    BindTexture(dev, 1, m_history[write], false);
    BindTexture(dev, 2, m_rays[1], true);
    BindTexture(dev, 3, sceneBlend ? m_sceneCopy : nullptr, false);
    DrawFullscreen(dev);

    if (DepthProbeDue(now))
    {
        m_probeTicks = now;
        ++m_probeAttempts;
        LogDepthProbe(dev, depthTexture, m_history[write], vp, worldDepth.deepest, in.dayFraction);
    }

    std::memcpy(m_prevWorldToView, worldToView, sizeof(m_prevWorldToView));
    std::memcpy(m_prevProj, in.glProjection, sizeof(m_prevProj));
    std::memcpy(m_prevCam, in.camPos, sizeof(m_prevCam));
    m_prevViewport = vp;
    m_prevScale = scale;
    m_prevConfig = cfg;
    m_prevMap = in.mapId;
    m_prevLightSlot = in.lightParams.screenEffectSlot;
    m_prevShadowMode = shadowMode;
    m_prevLocalLightCount = pointLightCount;
    m_prevTicks = now;
    m_historyIndex = write;
    m_historyValid = true;
    ++m_frame;
    return true;
}
