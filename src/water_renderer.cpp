#include "water_renderer.h"

#include "fog_model.h"
#include "log.h"
#include "water_classify.h"
#include "water_data.h"

#include "ps_vw_depth.h"
#include "ps_vw_depth_packed.h"
#include "ps_vw_shade_high.h"
#include "ps_vw_shade_low.h"
#include "ps_vw_shade_mid.h"
#include "vs_fullscreen.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
constexpr UINT kWaterPixelConstants = 72;
constexpr DWORD kWaterSamplerStages = 16;
constexpr UINT kCommonConstants = 9;
constexpr UINT kShadingFirstConstant = 9;
constexpr DWORD kMaxRenderTargets = 4;
constexpr DWORD kSceneColourStage = 0;
constexpr DWORD kSceneDepthStage = 1;
constexpr DWORD kWaterDepthStage = 2;
constexpr DWORD kFirstSurfaceStage = 3;
constexpr DWORD kFirstFoamStateStage = 7;
constexpr DWORD kFirstMaskStage = 11;
constexpr DWORD kStencilAllBits = 0xFF;
constexpr DWORD kColourWriteRgb = D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE;
constexpr DWORD kColourWriteAll = kColourWriteRgb | D3DCOLORWRITEENABLE_ALPHA;
constexpr float kWaterMaxViewDepth = 65536.0f;
constexpr float kMinViewportDepthExtent = 0.01f;
constexpr float kDeepestWorldDepthInFullRangeViewport = 0.9999995f;
constexpr float kWorldDepthMargin = 2.0e-6f;
constexpr UINT kMinWorldViewportSide = 16;
constexpr float kPackedDepthLevels = 16777215.0f;
constexpr float kByteMax = 255.0f;
constexpr float kHighByteWeight = 65536.0f;
constexpr float kMidByteWeight = 256.0f;
constexpr float kDisplayGamma = 2.2f;
constexpr float kSunVisibilityHalfWidth = 0.1f;
constexpr float kWaterF0 = 0.02f;
constexpr int kSchlickExponent = 5;
constexpr float kMinClarity = 0.01f;
constexpr float kShoreDistancePerDepth = 4.0f;
constexpr float kMinStockFogRange = 1e-3f;
constexpr double kMaxFoamStepSeconds = 0.1;
constexpr double kSummarySeconds = 60.0;
constexpr size_t kSummaryTextSize = 96;
constexpr int kLowQuality = 1;
constexpr int kSkyReflectionsOnlyVariant = 0;
constexpr int kMaskUploadRetryPasses = 60;
constexpr float kReflectionFogActive = 1.0f;
constexpr int kFftResolutionLow = 128;
constexpr int kFftResolution = 256;
constexpr int kFftReferenceResolution = 256;
constexpr float kWindDirection[2] = {0.8f, 0.6f};
constexpr int kSkyTop = 0;
constexpr int kSkyMiddle = 1;
constexpr int kSkyUpperBand = 2;
constexpr int kSkyLowerBand = 3;
constexpr int kSkyHorizon = 4;
constexpr int kSkyColourOfBand[kWaterSkyBands] = {kSkyTop, kSkyMiddle, kSkyUpperBand, kSkyLowerBand, kSkyHorizon};
constexpr int kCloseWaterColour = 0;
constexpr int kFarWaterColour = 1;

constexpr DWORD kUntaggedStencil = 0;

struct StencilSetting
{
    D3DRENDERSTATETYPE state;
    DWORD value;
};

const StencilSetting kPassStencil[kWaterStencilStates] = {
    {D3DRS_STENCILENABLE, TRUE},
    {D3DRS_STENCILFUNC, D3DCMP_ALWAYS},
    {D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE},
    {D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP},
    {D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP},
    {D3DRS_STENCILREF, kUntaggedStencil},
    {D3DRS_STENCILMASK, kStencilAllBits},
    {D3DRS_STENCILWRITEMASK, kStencilAllBits},
    {D3DRS_TWOSIDEDSTENCILMODE, FALSE},
};

const D3DRENDERSTATETYPE kPassRenderStates[] = {
    D3DRS_ZENABLE,           D3DRS_ZWRITEENABLE,     D3DRS_ZFUNC,
    D3DRS_ALPHATESTENABLE,   D3DRS_ALPHABLENDENABLE, D3DRS_SRCBLEND,
    D3DRS_DESTBLEND,         D3DRS_BLENDOP,          D3DRS_SEPARATEALPHABLENDENABLE,
    D3DRS_CULLMODE,          D3DRS_STENCILENABLE,    D3DRS_STENCILFUNC,
    D3DRS_STENCILPASS,       D3DRS_STENCILFAIL,      D3DRS_STENCILZFAIL,
    D3DRS_STENCILREF,        D3DRS_STENCILMASK,      D3DRS_STENCILWRITEMASK,
    D3DRS_TWOSIDEDSTENCILMODE, D3DRS_CCW_STENCILFUNC, D3DRS_CCW_STENCILPASS,
    D3DRS_CCW_STENCILFAIL,   D3DRS_CCW_STENCILZFAIL, D3DRS_SCISSORTESTENABLE,
    D3DRS_COLORWRITEENABLE,  D3DRS_SRGBWRITEENABLE,  D3DRS_FOGENABLE,
    D3DRS_CLIPPLANEENABLE,   D3DRS_FILLMODE,
};

const D3DSAMPLERSTATETYPE kPassSamplerStates[] = {
    D3DSAMP_ADDRESSU,   D3DSAMP_ADDRESSV,      D3DSAMP_ADDRESSW,    D3DSAMP_BORDERCOLOR,
    D3DSAMP_MAGFILTER,  D3DSAMP_MINFILTER,     D3DSAMP_MIPFILTER,   D3DSAMP_MIPMAPLODBIAS,
    D3DSAMP_MAXMIPLEVEL, D3DSAMP_MAXANISOTROPY, D3DSAMP_SRGBTEXTURE,
};

constexpr DWORD kInjectedWaterFault = 0xE0564657;

double g_secondsOverride = -1.0;
bool g_waveSimulationDisabled = false;
bool g_packedDepthForced = false;
WaterFaultStage g_injectedFault = WaterFaultStage::None;
int g_failingMaskUploads = 0;
int g_forcedShadingVariant = -1;
bool g_summaryForced = false;

void RaiseInjectedFault(WaterFaultStage stage)
{
    if (g_injectedFault != stage)
        return;
    g_injectedFault = WaterFaultStage::None;
    RaiseException(kInjectedWaterFault, 0, 0, nullptr);
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

double QpcSeconds()
{
    static const double frequency = [] {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return static_cast<double>(f.QuadPart);
    }();
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return static_cast<double>(now.QuadPart) / frequency;
}

double WaterSeconds()
{
    return g_secondsOverride >= 0.0 ? g_secondsOverride : QpcSeconds();
}

bool CreateTarget(IDirect3DDevice9* dev, UINT w, UINT h, D3DFORMAT format, IDirect3DTexture9** out)
{
    return SUCCEEDED(dev->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, format, D3DPOOL_DEFAULT, out, nullptr));
}

void SetTarget(IDirect3DDevice9* dev, IDirect3DTexture9* texture)
{
    IDirect3DSurface9* surface = nullptr;
    if (SUCCEEDED(texture->GetSurfaceLevel(0, &surface)))
    {
        dev->SetRenderTarget(0, surface);
        surface->Release();
    }
}

void BindSampler(IDirect3DDevice9* dev, DWORD stage, IDirect3DBaseTexture9* texture, D3DTEXTUREADDRESS address,
                 D3DTEXTUREFILTERTYPE filter, D3DTEXTUREFILTERTYPE mipFilter)
{
    dev->SetTexture(stage, texture);
    dev->SetSamplerState(stage, D3DSAMP_ADDRESSU, address);
    dev->SetSamplerState(stage, D3DSAMP_ADDRESSV, address);
    dev->SetSamplerState(stage, D3DSAMP_ADDRESSW, address);
    dev->SetSamplerState(stage, D3DSAMP_MAGFILTER, filter);
    dev->SetSamplerState(stage, D3DSAMP_MINFILTER, filter);
    dev->SetSamplerState(stage, D3DSAMP_MIPFILTER, mipFilter);
    dev->SetSamplerState(stage, D3DSAMP_MIPMAPLODBIAS, 0);
    dev->SetSamplerState(stage, D3DSAMP_MAXMIPLEVEL, 0);
    dev->SetSamplerState(stage, D3DSAMP_SRGBTEXTURE, FALSE);
}

void BindPointSampler(IDirect3DDevice9* dev, DWORD stage, IDirect3DBaseTexture9* texture, D3DTEXTUREADDRESS address)
{
    BindSampler(dev, stage, texture, address, D3DTEXF_POINT, D3DTEXF_NONE);
}

RECT ViewportRect(const D3DVIEWPORT9& vp)
{
    return {static_cast<LONG>(vp.X), static_cast<LONG>(vp.Y), static_cast<LONG>(vp.X + vp.Width),
            static_cast<LONG>(vp.Y + vp.Height)};
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

bool BuildCommonConstants(const FrameInputs& in, const D3DSURFACE_DESC& depthDesc, float (&common)[9][4])
{
    float viewToWorld[16];
    if (!Invert4x4(in.cameraRelativeView, viewToWorld))
        return false;
    viewToWorld[12] = in.camPos[0];
    viewToWorld[13] = in.camPos[1];
    viewToWorld[14] = in.camPos[2];
    const D3DVIEWPORT9& vp = in.viewport;
    const float* proj = in.glProjection;
    const WorldDepthMapping depth = MapWorldDepth(proj, vp);
    const float width = static_cast<float>(vp.Width);
    const float height = static_cast<float>(vp.Height);
    const float rows[9][4] = {
        {static_cast<float>(vp.X), static_cast<float>(vp.Y), width, height},
        {1.0f, 0.0f, 1.0f / depthDesc.Width, 1.0f / depthDesc.Height},
        {proj[0], proj[5], proj[8], proj[9]},
        {depth.atInfinity, depth.perInverseViewDepth, kWaterMaxViewDepth, depth.deepest},
        {},
        {},
        {},
        {},
        {width, height, 1.0f / width, 1.0f / height},
    };
    std::memcpy(common, rows, sizeof(rows));
    std::memcpy(common[4], viewToWorld, sizeof(viewToWorld));
    return true;
}

float Saturate(float x)
{
    return std::clamp(x, 0.0f, 1.0f);
}

float SmoothStep(float edge0, float edge1, float x)
{
    const float t = Saturate((x - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

float FresnelSchlick(float cosine)
{
    return kWaterF0 + (1.0f - kWaterF0) * std::pow(1.0f - Saturate(cosine), static_cast<float>(kSchlickExponent));
}

void LinearColour(uint32_t argb, float* rgb)
{
    UnpackColor(argb, rgb);
    for (int c = 0; c < 3; ++c)
        rgb[c] = std::pow(rgb[c], kDisplayGamma);
}

void ClampedTint(const float* linearTint, float* rgb)
{
    for (int c = 0; c < 3; ++c)
        rgb[c] = Saturate(linearTint[c]);
}

float ScrollOffset(double seconds, int axis, float multiplier)
{
    const double travelled = seconds * kWindDirection[axis] * multiplier;
    return static_cast<float>(travelled - std::floor(travelled));
}

uint32_t MipSide(uint32_t size, uint32_t level)
{
    const uint32_t side = size >> level;
    return side ? side : 1;
}

bool FillMaskLevel(IDirect3DTexture9* texture, UINT level, const std::vector<uint8_t>& texels, uint32_t side,
                   bool luminance)
{
    D3DLOCKED_RECT locked = {};
    if (FAILED(texture->LockRect(level, &locked, nullptr, 0)))
        return false;
    for (uint32_t y = 0; y < side; ++y)
    {
        const uint8_t* source = texels.data() + static_cast<size_t>(y) * side;
        auto* row = static_cast<uint8_t*>(locked.pBits) + static_cast<size_t>(y) * locked.Pitch;
        if (luminance)
        {
            std::memcpy(row, source, side);
            continue;
        }
        auto* argb = reinterpret_cast<DWORD*>(row);
        for (uint32_t x = 0; x < side; ++x)
            argb[x] = 0xFF000000u | source[x] * 0x00010101u;
    }
    return SUCCEEDED(texture->UnlockRect(level));
}

IDirect3DTexture9* CreateMaskStaging(IDirect3DDevice9* dev, const WaterMaskLevels& mask, D3DFORMAT format)
{
    const UINT levels = static_cast<UINT>(mask.levels.size());
    const UINT size = mask.info.size;
    IDirect3DTexture9* staging = nullptr;
    if (FAILED(dev->CreateTexture(size, size, levels, 0, format, D3DPOOL_SYSTEMMEM, &staging, nullptr)))
        return nullptr;
    for (UINT level = 0; level < levels; ++level)
        if (!FillMaskLevel(staging, level, mask.levels[level], MipSide(size, level), format == D3DFMT_L8))
        {
            staging->Release();
            return nullptr;
        }
    return staging;
}

IDirect3DTexture9* CreateMaskTexture(IDirect3DDevice9* dev, const WaterMaskLevels& mask)
{
    if (g_failingMaskUploads > 0)
    {
        --g_failingMaskUploads;
        return nullptr;
    }
    const UINT levels = static_cast<UINT>(mask.levels.size());
    const UINT size = mask.info.size;
    for (D3DFORMAT format : {D3DFMT_L8, D3DFMT_A8R8G8B8})
    {
        IDirect3DTexture9* texture = nullptr;
        if (FAILED(dev->CreateTexture(size, size, levels, 0, format, D3DPOOL_DEFAULT, &texture, nullptr)))
            continue;
        IDirect3DTexture9* staging = CreateMaskStaging(dev, mask, format);
        const bool uploaded = staging && SUCCEEDED(dev->UpdateTexture(staging, texture));
        if (staging)
            staging->Release();
        if (uploaded)
            return texture;
        texture->Release();
    }
    return nullptr;
}

int QualityIndex(const Config& cfg)
{
    return std::clamp(cfg.waterQuality, 1, kWaterQualityLevels) - 1;
}

int ShadingVariant(const Config& cfg)
{
    return cfg.waterReflections > 0.0f ? QualityIndex(cfg) : kSkyReflectionsOnlyVariant;
}
}

void OverrideWaterSeconds(double seconds)
{
    g_secondsOverride = seconds;
}

void DisableWaveSimulation(bool disabled)
{
    g_waveSimulationDisabled = disabled;
}

void ForcePackedWaterDepth(bool forced)
{
    g_packedDepthForced = forced;
}

void InjectWaterFault(WaterFaultStage stage)
{
    g_injectedFault = stage;
}

void FailWaterMaskUploads(int count)
{
    g_failingMaskUploads = count;
}

void ForceWaterShadingVariant(int variant)
{
    g_forcedShadingVariant = variant;
}

void ForceWaterSummary()
{
    g_summaryForced = true;
}

WaterRenderer::~WaterRenderer()
{
    ReleaseAll();
}

void WaterRenderer::ReleaseDefaultPool()
{
    ReleaseTargets();
    m_gpuTimer.Release();
    ReleaseMasks();
    SafeRelease(m_sceneColour);
    SafeRelease(m_sceneDepth);
    SafeRelease(m_waterDepth);
    SafeRelease(m_state);
    m_copyW = m_copyH = 0;
    m_copyFailed = false;
    m_armed = false;
    m_stencilArmed = false;
    m_lastSeconds = -1.0;
    m_fft.ReleaseDefaultPool();
}

void WaterRenderer::ReleaseAll()
{
    ReleaseDefaultPool();
    m_unsupportedShaderDevice = nullptr;
    SafeRelease(m_vs);
    SafeRelease(m_decl);
    SafeRelease(m_depthCopy);
    SafeRelease(m_packedDepthCopy);
    for (auto*& shader : m_shade)
        SafeRelease(shader);
    SafeRelease(m_flat);
    m_fft.ReleaseAll();
}

void WaterRenderer::ReleaseMasks()
{
    for (auto*& mask : m_masks)
        SafeRelease(mask);
    m_masks.clear();
    m_masksUploaded = false;
    m_maskRetryPasses = 0;
}

bool WaterRenderer::Skip(const char* reason)
{
    m_skip = reason;
    return false;
}

bool WaterRenderer::EnsureShaders(IDirect3DDevice9* dev)
{
    if (m_unsupportedShaderDevice == dev)
        return Skip("water shader unsupported");
    if (m_vs)
        return true;
    struct PixelShaderRequest
    {
        const char* name;
        const BYTE* code;
        IDirect3DPixelShader9** output;
    };
    const PixelShaderRequest pixels[] = {
        {"ps_vw_depth", g_ps_vw_depth, &m_depthCopy},
        {"ps_vw_depth_packed", g_ps_vw_depth_packed, &m_packedDepthCopy},
        {"ps_vw_shade_low", g_ps_vw_shade_low, &m_shade[0]},
        {"ps_vw_shade_mid", g_ps_vw_shade_mid, &m_shade[1]},
        {"ps_vw_shade_high", g_ps_vw_shade_high, &m_shade[2]},
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
        failedName = "water vertex declaration";
        result = dev->CreateVertexDeclaration(kElements, &m_decl);
    }
    if (SUCCEEDED(result))
        return true;
    const bool unsupported = (result == D3DERR_INVALIDCALL || result == D3DERR_NOTAVAILABLE ||
                              result == E_INVALIDARG) && dev->TestCooperativeLevel() == D3D_OK;
    VF_LOG_ERROR("water shader initialization failed: %s HRESULT 0x%08lX; %s", failedName,
                 static_cast<unsigned long>(result), unsupported ? "unsupported on this device" : "will retry");
    ReleaseAll();
    if (unsupported)
        m_unsupportedShaderDevice = dev;
    return Skip(unsupported ? "water shader unsupported" : "water shader creation failed");
}

bool WaterRenderer::EnsureStateBlock(IDirect3DDevice9* dev)
{
    if (m_state)
        return true;
    if (FAILED(dev->BeginStateBlock()))
        return Skip("water state block recording failed");
    for (D3DRENDERSTATETYPE state : kPassRenderStates)
        dev->SetRenderState(state, 0);
    for (DWORD stage = 0; stage < kWaterSamplerStages; ++stage)
    {
        dev->SetTexture(stage, nullptr);
        for (D3DSAMPLERSTATETYPE state : kPassSamplerStates)
            dev->SetSamplerState(stage, state, 0);
    }
    float zeros[kWaterPixelConstants * 4] = {};
    dev->SetVertexShader(nullptr);
    dev->SetPixelShader(nullptr);
    dev->SetPixelShaderConstantF(0, zeros, kWaterPixelConstants);
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
        return Skip("water state block recording failed");
    }
    return true;
}

bool WaterRenderer::EnsureCopies(IDirect3DDevice9* dev, IDirect3DSurface9* target, UINT w, UINT h)
{
    if (m_sceneColour && m_copyW == w && m_copyH == h && m_packedDepthForcedCopies == g_packedDepthForced)
        return true;
    SafeRelease(m_sceneColour);
    SafeRelease(m_sceneDepth);
    SafeRelease(m_waterDepth);
    m_copyW = m_copyH = 0;
    if (m_copyFailed)
        return Skip("water copies unavailable");
    D3DSURFACE_DESC desc = {};
    target->GetDesc(&desc);
    const bool colour = CreateTarget(dev, w, h, desc.Format, &m_sceneColour) ||
                        CreateTarget(dev, w, h, D3DFMT_A8R8G8B8, &m_sceneColour);
    m_packedDepth = g_packedDepthForced || !CreateTarget(dev, w, h, D3DFMT_R32F, &m_sceneDepth) ||
                    !CreateTarget(dev, w, h, D3DFMT_R32F, &m_waterDepth);
    bool depth = !m_packedDepth;
    if (m_packedDepth)
    {
        SafeRelease(m_sceneDepth);
        SafeRelease(m_waterDepth);
        depth = CreateTarget(dev, w, h, D3DFMT_A8R8G8B8, &m_sceneDepth) &&
                CreateTarget(dev, w, h, D3DFMT_A8R8G8B8, &m_waterDepth);
    }
    if (!colour || !depth)
    {
        SafeRelease(m_sceneColour);
        SafeRelease(m_sceneDepth);
        SafeRelease(m_waterDepth);
        m_copyFailed = true;
        VF_LOG_ERROR("water scene copies could not be created (%ux%u); water keeps the client's shading", w, h);
        return Skip("water copy creation failed");
    }
    m_copyW = w;
    m_copyH = h;
    m_packedDepthForcedCopies = g_packedDepthForced;
    const bool newCopies = w != m_loggedCopyW || h != m_loggedCopyH || m_packedDepth != m_loggedPackedDepth;
    m_loggedCopyW = w;
    m_loggedCopyH = h;
    m_loggedPackedDepth = m_packedDepth;
    LogWrite(newCopies ? LogLevel::Info : LogLevel::Debug, "water copies: %ux%u, depth %s", w, h,
             m_packedDepth ? "packed rgba8" : "r32f");
    return true;
}

bool WaterRenderer::EnsureFlatTexture(IDirect3DDevice9* dev)
{
    if (m_flat)
        return true;
    if (FAILED(dev->CreateTexture(1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &m_flat, nullptr)))
    {
        m_flat = nullptr;
        return Skip("flat water texture creation failed");
    }
    D3DLOCKED_RECT locked = {};
    if (FAILED(m_flat->LockRect(0, &locked, nullptr, 0)))
    {
        SafeRelease(m_flat);
        return Skip("flat water texture creation failed");
    }
    *static_cast<DWORD*>(locked.pBits) = 0;
    m_flat->UnlockRect(0);
    return true;
}

void WaterRenderer::EnsureMasks(IDirect3DDevice9* dev)
{
    const WaterData& data = GlobalWaterData();
    if (m_maskRevision != data.Revision() || m_masks.size() != data.Masks().size())
    {
        ReleaseMasks();
        m_masks.assign(data.Masks().size(), nullptr);
        m_maskRevision = data.Revision();
    }
    if (m_masksUploaded)
        return;
    if (m_maskRetryPasses > 0)
    {
        --m_maskRetryPasses;
        return;
    }
    int uploaded = 0;
    for (size_t i = 0; i < m_masks.size(); ++i)
    {
        if (!m_masks[i])
            m_masks[i] = CreateMaskTexture(dev, data.Masks()[i]);
        uploaded += m_masks[i] ? 1 : 0;
    }
    m_masksUploaded = uploaded == static_cast<int>(m_masks.size());
    m_maskRetryPasses = m_masksUploaded ? 0 : kMaskUploadRetryPasses;
    const bool newData = !m_masksLogged || m_loggedMaskRevision != data.Revision() || uploaded != m_loggedMaskUploads;
    m_loggedMaskRevision = data.Revision();
    m_masksLogged = true;
    m_loggedMaskUploads = uploaded;
    LogWrite(newData ? LogLevel::Info : LogLevel::Debug, "water foam masks uploaded: %d of %d%s", uploaded,
             static_cast<int>(m_masks.size()), m_masksUploaded ? "" : "; retrying the others later");
}

unsigned WaterRenderer::HeldResources() const
{
    unsigned held = 0;
    if (m_sceneColour || m_sceneDepth || m_waterDepth)
        held |= kWaterSceneCopiesHeld;
    if (m_fft.HoldsDeviceResources())
        held |= kWaterWaveMapsHeld;
    for (IDirect3DTexture9* mask : m_masks)
        if (mask)
            held |= kWaterFoamMasksHeld;
    return held;
}

int WaterRenderer::FoamMaskPool() const
{
    for (IDirect3DTexture9* mask : m_masks)
    {
        D3DSURFACE_DESC desc = {};
        if (mask && SUCCEEDED(mask->GetLevelDesc(0, &desc)))
            return static_cast<int>(desc.Pool);
    }
    return -1;
}

int WaterRenderer::UploadedMasks() const
{
    return static_cast<int>(std::count_if(m_masks.begin(), m_masks.end(), [](IDirect3DTexture9* mask) {
        return mask != nullptr;
    }));
}

IDirect3DTexture9* WaterRenderer::MaskTexture(int32_t index) const
{
    return index >= 0 && static_cast<size_t>(index) < m_masks.size() ? m_masks[index] : nullptr;
}

void WaterRenderer::SaveTargets(IDirect3DDevice9* dev)
{
    ReleaseTargets();
    for (DWORD i = 0; i < kMaxRenderTargets; ++i)
        dev->GetRenderTarget(i, &m_saved.colour[i]);
    dev->GetDepthStencilSurface(&m_saved.depth);
    dev->GetStreamSource(0, &m_saved.stream, &m_saved.streamOffset, &m_saved.streamStride);
}

void WaterRenderer::ReleaseTargets()
{
    m_stateCaptured = false;
    for (auto*& surface : m_saved.colour)
        SafeRelease(surface);
    SafeRelease(m_saved.depth);
    SafeRelease(m_saved.stream);
}

void WaterRenderer::CaptureClientState()
{
    m_state->Capture();
    m_stateCaptured = true;
}

void WaterRenderer::RestoreTargets(IDirect3DDevice9* dev)
{
    const bool captured = m_stateCaptured;
    m_stateCaptured = false;
    dev->SetRenderTarget(0, m_saved.colour[0]);
    for (DWORD i = 1; i < kMaxRenderTargets; ++i)
        dev->SetRenderTarget(i, m_saved.colour[i]);
    if (captured && m_state)
        m_state->Apply();
    dev->SetDepthStencilSurface(m_saved.depth);
    dev->SetStreamSource(0, m_saved.stream, m_saved.streamOffset, m_saved.streamStride);
    ReleaseTargets();
}

bool WaterRenderer::UsableTargets(IDirect3DSurface9* depthSurface, const D3DVIEWPORT9& vp, D3DSURFACE_DESC& depthDesc)
{
    const SavedTargets& saved = m_saved;
    D3DSURFACE_DESC rtDesc = {};
    if (!saved.colour[0])
        return Skip("no render target");
    if (saved.depth != depthSurface)
        return Skip("fog depth surface not bound");
    if (FAILED(saved.colour[0]->GetDesc(&rtDesc)) || FAILED(depthSurface->GetDesc(&depthDesc)))
        return Skip("surface description failed");
    if (rtDesc.Width != depthDesc.Width || rtDesc.Height != depthDesc.Height)
        return Skip("render target and depth sizes differ");
    if (!SameSampleCount(rtDesc, depthDesc))
        return Skip("render target and depth sample counts differ");
    if (vp.Width < kMinWorldViewportSide || vp.Height < kMinWorldViewportSide || vp.X + vp.Width > depthDesc.Width ||
        vp.Y + vp.Height > depthDesc.Height)
        return Skip("world viewport outside the render target");
    return true;
}

void WaterRenderer::SetPassState(IDirect3DDevice9* dev)
{
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
    dev->SetRenderState(D3DRS_COLORWRITEENABLE, kColourWriteAll);
    dev->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
    dev->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
    dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
}

void WaterRenderer::DrawFullscreen(IDirect3DDevice9* dev)
{
    static const float kTriangle[3][4] = {
        {-1.0f, -1.0f, 0.0f, 1.0f}, {-1.0f, 3.0f, 0.0f, 1.0f}, {3.0f, -1.0f, 0.0f, 1.0f}};
    dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, kTriangle, sizeof(kTriangle[0]));
}

bool WaterRenderer::CopySceneColour(IDirect3DDevice9* dev, IDirect3DSurface9* target, const D3DVIEWPORT9& vp)
{
    const RECT world = ViewportRect(vp);
    IDirect3DSurface9* copy = nullptr;
    const bool copied = SUCCEEDED(m_sceneColour->GetSurfaceLevel(0, &copy)) &&
                        SUCCEEDED(dev->StretchRect(target, &world, copy, nullptr, D3DTEXF_POINT));
    SafeRelease(copy);
    return copied;
}

void WaterRenderer::CopyLinearDepth(IDirect3DDevice9* dev, IDirect3DTexture9* depthTexture, IDirect3DTexture9* copy)
{
    SetTarget(dev, copy);
    dev->SetPixelShader(m_packedDepth ? m_packedDepthCopy : m_depthCopy);
    dev->SetPixelShaderConstantF(0, &m_common[0][0], kCommonConstants);
    BindPointSampler(dev, 0, depthTexture, D3DTADDRESS_CLAMP);
    DrawFullscreen(dev);
}

void WaterRenderer::ClearWaterStencil(IDirect3DDevice9* dev, IDirect3DSurface9* target,
                                      IDirect3DSurface9* depthSurface, const D3DVIEWPORT9& vp)
{
    dev->SetRenderTarget(0, target);
    dev->SetDepthStencilSurface(depthSurface);
    dev->SetViewport(&vp);
    dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    dev->SetRenderState(D3DRS_STENCILWRITEMASK, kStencilAllBits);
    const D3DRECT world = {static_cast<LONG>(vp.X), static_cast<LONG>(vp.Y), static_cast<LONG>(vp.X + vp.Width),
                           static_cast<LONG>(vp.Y + vp.Height)};
    dev->Clear(1, &world, D3DCLEAR_STENCIL, 0, 1.0f, 0);
}

void WaterRenderer::ArmStencilWrites(IDirect3DDevice9* dev)
{
    for (int i = 0; i < kWaterStencilStates; ++i)
    {
        dev->GetRenderState(kPassStencil[i].state, &m_clientStencil[i]);
        dev->SetRenderState(kPassStencil[i].state, kPassStencil[i].value);
    }
    m_stencilArmed = true;
}

void WaterRenderer::RestoreClientStencil(IDirect3DDevice9* dev)
{
    if (!m_stencilArmed)
        return;
    m_stencilArmed = false;
    if (!dev)
        return;
    for (int i = 0; i < kWaterStencilStates; ++i)
        dev->SetRenderState(kPassStencil[i].state, m_clientStencil[i]);
}

bool WaterRenderer::Begin(IDirect3DDevice9* dev, const SceneDepth& depth, const FrameInputs& in,
                          const WaterInputs& water, const Config& cfg)
{
    m_skip = "";
    if (m_armed)
        Abort(dev);
    if (!cfg.water)
        return Skip("water disabled");
    if (in.inLiquid)
        return Skip("camera under water");
    if (!GlobalWaterData().Loaded())
        return Skip("no water data");
    if (!dev || !depth.texture || !depth.bound)
        return Skip("fog depth unavailable");
    if (dev->TestCooperativeLevel() != D3D_OK)
        return Skip("device not ready");
    if (!EnsureShaders(dev) || !EnsureStateBlock(dev) || !EnsureFlatTexture(dev))
        return false;

    SaveTargets(dev);
    D3DSURFACE_DESC depthDesc = {};
    const D3DVIEWPORT9& vp = in.viewport;
    if (!UsableTargets(depth.bound, vp, depthDesc) || !EnsureCopies(dev, m_saved.colour[0], vp.Width, vp.Height))
    {
        ReleaseTargets();
        return false;
    }
    if (!BuildCommonConstants(in, depthDesc, m_common))
    {
        ReleaseTargets();
        return Skip("view matrix not invertible");
    }
    EnsureMasks(dev);

    if (LogEnabled(LogLevel::Info))
        m_gpuTimer.Begin(dev);
    CaptureClientState();
    for (DWORD i = 1; i < kMaxRenderTargets; ++i)
        dev->SetRenderTarget(i, nullptr);
    const bool colourCopied = CopySceneColour(dev, m_saved.colour[0], vp);
    const bool copied = colourCopied && depth.Refresh(dev);
    if (copied)
    {
        dev->SetDepthStencilSurface(nullptr);
        SetPassState(dev);
        RaiseInjectedFault(WaterFaultStage::Begin);
        CopyLinearDepth(dev, depth.texture, m_sceneDepth);
        ClearWaterStencil(dev, m_saved.colour[0], depth.bound, vp);
    }
    RestoreTargets(dev);
    if (!copied)
    {
        m_gpuTimer.Cancel();
        return Skip(colourCopied ? "multisampled depth copy failed" : "scene colour copy failed");
    }
    m_gpuTimer.Pause();

    ArmStencilWrites(dev);
    m_in = in;
    m_water = water;
    m_cfg = cfg;
    std::fill(std::begin(m_draws), std::end(m_draws), 0u);
    m_armed = true;
    return true;
}

void WaterRenderer::Tag(IDirect3DDevice9* dev, WaterClass waterClass)
{
    const int index = static_cast<int>(waterClass);
    if (!m_stencilArmed || index < 0 || index >= kWaterClassCount)
        return;
    dev->SetRenderState(D3DRS_STENCILREF, static_cast<DWORD>(index));
    ++m_draws[index];
}

void WaterRenderer::Untag(IDirect3DDevice9* dev)
{
    if (m_stencilArmed)
        dev->SetRenderState(D3DRS_STENCILREF, kUntaggedStencil);
}

void WaterRenderer::Abort(IDirect3DDevice9* dev)
{
    if (dev && m_stateCaptured)
        RestoreTargets(dev);
    else
        ReleaseTargets();
    RestoreClientStencil(dev);
    m_gpuTimer.Cancel();
    m_armed = false;
}

bool WaterRenderer::AnyClassDrawn() const
{
    for (int index = 1; index < kWaterClassCount; ++index)
        if (m_draws[index])
            return true;
    return false;
}

uint32_t WaterRenderer::DrawnTileMask() const
{
    const WaterData& data = GlobalWaterData();
    uint32_t mask = 0;
    for (int index = 1; index < kWaterClassCount; ++index)
    {
        const WaterPreset* preset = m_draws[index] ? data.Preset(static_cast<WaterClass>(index)) : nullptr;
        if (!preset)
            continue;
        for (int32_t tile : preset->tiles)
            if (tile >= 0 && tile < kWaterMaxTiles && static_cast<size_t>(tile) < data.Tiles().size())
                mask |= 1u << tile;
    }
    return mask;
}

bool WaterRenderer::PrepareWaves(IDirect3DDevice9* dev)
{
    m_waveTiles = DrawnTileMask();
    m_waveSettings = {};
    m_waveSettings.resolution = m_cfg.waterQuality == kLowQuality ? kFftResolutionLow : kFftResolution;
    m_waveSettings.referenceResolution = kFftReferenceResolution;
    m_waveSettings.windSpeed = m_cfg.waterWind;
    m_waveSettings.windDirection[0] = kWindDirection[0];
    m_waveSettings.windDirection[1] = kWindDirection[1];
    m_waveSettings.amplitudeScale = m_cfg.waterWaves * m_cfg.waterWaves;
    m_wavesAttempted = !g_waveSimulationDisabled && m_waveTiles != 0 && m_cfg.waterWaves > 0.0f;
    return m_wavesAttempted && m_fft.Prepare(dev, m_waveSettings, GlobalWaterData().Tiles(), m_waveTiles);
}

bool WaterRenderer::SimulateWaves(IDirect3DDevice9* dev, double seconds)
{
    const double step = m_lastSeconds < 0.0 ? 0.0 : std::clamp(seconds - m_lastSeconds, 0.0, kMaxFoamStepSeconds);
    return m_fft.Run(dev, m_waveSettings, GlobalWaterData().Tiles(), seconds, static_cast<float>(step));
}

void WaterRenderer::LogWaveState()
{
    if (!m_wavesAttempted)
        return;
    if (m_wavesSimulated)
        m_fft.LogPlan();
    const char* failure = m_fft.LastFailure();
    const char* state = m_wavesSimulated ? "" : (failure && *failure ? failure : "unknown failure");
    if (m_waveStateLogged && std::strcmp(state, m_loggedWaveState) == 0)
        return;
    if (m_wavesSimulated)
        VF_LOG_INFO("water waves simulated (tiles 0x%02X, %d texels)", m_waveTiles, m_waveSettings.resolution);
    else
        VF_LOG_INFO("water waves unavailable, shading flat water: %s", state);
    m_loggedWaveState = state;
    m_waveStateLogged = true;
}

void WaterRenderer::FillClassConstants(ShadingConstants& c, const WaterPreset& preset, WaterClass waterClass,
                                       double seconds) const
{
    c = {};
    const WaterData& data = GlobalWaterData();
    const bool noSun = waterClass == WaterClass::Interior;
    float toLight[3] = {m_in.toLight[0], m_in.toLight[1], m_in.toLight[2]};
    const float length = std::sqrt(toLight[0] * toLight[0] + toLight[1] * toLight[1] + toLight[2] * toLight[2]);
    for (float& axis : toLight)
        axis = length > 1e-6f ? axis / length : 0.0f;
    const float sunVisibility =
        noSun ? 0.0f : SmoothStep(-kSunVisibilityHalfWidth, kSunVisibilityHalfWidth, toLight[2]);
    const float sunTransmission = noSun ? 1.0f : 1.0f - FresnelSchlick(Saturate(toLight[2]));
    float sun[3];
    LinearColour(m_in.directColor, sun);
    for (float& channel : sun)
        channel *= m_in.lightIsMoon ? kMoonLightScale : 1.0f;
    float sky[kSkyColorCount][3];
    float ambient[3] = {};
    for (int i = 0; i < kSkyColorCount; ++i)
    {
        LinearColour(m_water.skyColors[i], sky[i]);
        for (int channel = 0; channel < 3; ++channel)
            ambient[channel] += sky[i][channel] / kSkyColorCount;
    }
    if (noSun)
        LinearColour(m_in.ambientColor, ambient);

    c.light = {toLight[0], toLight[1], toLight[2], sunVisibility};
    c.sunColour = {sun[0], sun[1], sun[2], sunTransmission};
    c.ambient = {ambient[0], ambient[1], ambient[2], 0.0f};
    c.isotropicLight = noSun ? Float4{1.0f, 1.0f, 1.0f, 0.0f}
                             : Float4{ambient[0] + sunVisibility * sun[0], ambient[1] + sunVisibility * sun[1],
                                      ambient[2] + sunVisibility * sun[2], 0.0f};
    for (int band = 0; band < kWaterSkyBands; ++band)
    {
        const float* colour = sky[kSkyColourOfBand[band]];
        c.sky[band] = noSun ? Float4{} : Float4{colour[0], colour[1], colour[2], 0.0f};
    }
    if (m_water.stockFogApplies)
    {
        float fog[3];
        UnpackColor(m_in.fogColor, fog);
        c.stockFogColour = {fog[0], fog[1], fog[2], 1.0f};
        c.stockFog = {m_in.fogEnd, 1.0f / std::max(m_in.fogEnd - m_in.fogStart, kMinStockFogRange), 0.0f, 0.0f};
    }

    const float absorptionScale = preset.absorption[3] / std::max(m_cfg.waterClarity, kMinClarity);
    c.absorption = {preset.absorption[0] * absorptionScale, preset.absorption[1] * absorptionScale,
                    preset.absorption[2] * absorptionScale, 0.0f};
    c.scatteringIntensities = {preset.scatteringIntensities[0], preset.scatteringIntensities[1],
                               preset.scatteringIntensities[2], preset.scatteringIntensities[3]};
    const uint32_t* zone = waterClass == WaterClass::Ocean ? m_water.oceanColors : m_water.riverColors;
    float closeWater[3];
    float farWater[3];
    LinearColour(zone[kCloseWaterColour], closeWater);
    LinearColour(zone[kFarWaterColour], farWater);
    const float zoneWeight = Saturate(m_cfg.waterZoneColors);
    auto towardZone = [zoneWeight](const float* authored, const float* zoneColour, float w) {
        return Float4{authored[0] + (zoneColour[0] - authored[0]) * zoneWeight,
                      authored[1] + (zoneColour[1] - authored[1]) * zoneWeight,
                      authored[2] + (zoneColour[2] - authored[2]) * zoneWeight, w};
    };
    c.scatteringTop = towardZone(preset.scatteringTop, closeWater, preset.scatteringTop[3]);
    c.scatteringBottom = towardZone(preset.scatteringBottom, farWater, preset.scatteringBottom[3]);

    const float foam = std::max(m_cfg.waterFoam, 0.0f);
    c.depthFadeFoam = {std::max(preset.depthFadeFoam[0], 0.0f) * foam, preset.depthFadeFoam[1],
                       preset.depthFadeFoam[2], 0.0f};
    c.shoreFoam = {std::max(preset.shoreFoam[0], 0.0f) * foam, preset.shoreFoam[1], preset.shoreFoam[2],
                   kShoreDistancePerDepth};
    c.waveFoam = {std::max(preset.waveFoam[0], 0.0f) * foam, 0.0f, 0.0f, 0.0f};
    c.waveFoamScaling = {preset.waveFoamScaling[0], preset.waveFoamScaling[1], preset.waveFoamScaling[2], 0.0f};
    c.surfaceResponse = {preset.roughness[0], preset.roughness[1], preset.roughness[2] * m_cfg.waterReflections,
                         m_cfg.waterSpecular};

    float inverse[kWaterPresetTiles] = {};
    int tileCount = 0;
    for (int k = 0; k < kWaterPresetTiles; ++k)
    {
        const int32_t tile = preset.tiles[k];
        if (tile < 0 || static_cast<size_t>(tile) >= data.Tiles().size() || !(data.Tiles()[tile].size > 0.0f))
            continue;
        inverse[k] = 1.0f / data.Tiles()[tile].size;
        ++tileCount;
    }
    c.inverseTileSizes = {inverse[0], inverse[1], inverse[2], inverse[3]};
    c.waveControl = {1.0f / static_cast<float>(std::max(tileCount, 1)), 0.0f, static_cast<float>(waterClass),
                     static_cast<float>(m_cfg.waterDebugView)};
    c.foamScroll = {ScrollOffset(seconds, 0, preset.waveFoam[1]), ScrollOffset(seconds, 1, preset.waveFoam[1]),
                    ScrollOffset(seconds, 0, preset.shoreFoam[3]), ScrollOffset(seconds, 1, preset.shoreFoam[3])};
    c.depthFoamScroll = {ScrollOffset(seconds, 0, preset.depthFadeFoam[3]),
                         ScrollOffset(seconds, 1, preset.depthFadeFoam[3]), 0.0f, 0.0f};
    if (!noSun)
        c.reflectionFog = m_reflectionFog;
    const float packedUnit = kWaterMaxViewDepth / kPackedDepthLevels * kByteMax;
    c.depthDecode = m_packedDepth ? Float4{packedUnit * kHighByteWeight, packedUnit * kMidByteWeight, packedUnit, 0.0f}
                                  : Float4{1.0f, 0.0f, 0.0f, 0.0f};
    for (int slot = 0; slot < kWaterShadedMaskSlots; ++slot)
    {
        const int32_t mask = preset.masks[slot];
        if (!MaskTexture(mask))
            continue;
        const WaterMask& info = data.Masks()[mask].info;
        float low[3];
        float high[3];
        ClampedTint(info.tintLow, low);
        ClampedTint(info.tintHigh, high);
        c.maskTints[slot * 2] = {low[0], low[1], low[2], 1.0f};
        c.maskTints[slot * 2 + 1] = {high[0], high[1], high[2], 0.0f};
    }
}

void WaterRenderer::BuildReflectionFog()
{
    m_reflectionFog = {};
    if (m_water.stockFogApplies)
        return;
    AuthoredFog authored = {};
    const bool hasAuthored = m_cfg.dataMode == 1 &&
                             GlobalFogData().Resolve(m_in.mapId, m_in.camPos, m_in.dayFraction, m_in.lightParams,
                                                     authored);
    const FogParams fog = BuildFogParams(m_in, m_cfg, hasAuthored ? &authored : nullptr);
    const float exposure = fog.authored ? m_cfg.classicExposure : m_cfg.exposure;
    auto linearScattered = [&fog, exposure](const float* rgb) {
        float out[3];
        for (int c = 0; c < 3; ++c)
            out[c] = (fog.linear ? rgb[c] : std::pow(std::max(rgb[c], 0.0f), kDisplayGamma)) * exposure;
        return Float4{out[0], out[1], out[2], 0.0f};
    };
    for (int i = 0; i < kFogLayers; ++i)
    {
        const FogLayer& layer = fog.layers[i];
        const float shadow = layer.shadowed * (1.0f - fog.lightAboveHorizon);
        float emissive[3];
        float diffuse[3];
        for (int c = 0; c < 3; ++c)
        {
            emissive[c] = layer.emissive[c] + (layer.shadowEmissive[c] - layer.emissive[c]) * shadow;
            diffuse[c] = layer.diffuse[c] * (1.0f - shadow);
        }
        const float density = layer.density * (1.0f + (layer.shadowDensity - 1.0f) * shadow);
        ReflectionFogLayer& out = m_reflectionFog.layers[i];
        out.curve = {layer.start, density, layer.strength, layer.exponent};
        out.height = {layer.upperHeight, layer.upperFalloff, layer.lowerHeight, layer.lowerFalloff};
        out.emissive = linearScattered(emissive);
        out.diffuse = linearScattered(diffuse);
        out.scattering = {layer.g, layer.isotropic, layer.endDistance, layer.skyFalloff};
    }
    m_reflectionFog.range = {fog.maxDistance, fog.maxDistance, 0.0f, kReflectionFogActive};
}

void WaterRenderer::BindClassTextures(IDirect3DDevice9* dev, const WaterPreset& preset)
{
    for (int k = 0; k < kWaterPresetTiles; ++k)
    {
        const int32_t tile = preset.tiles[k];
        IDirect3DTexture9* surface = m_wavesSimulated && tile >= 0 ? m_fft.Surface(tile) : nullptr;
        IDirect3DTexture9* foam = m_wavesSimulated && tile >= 0 ? m_fft.Foam(tile) : nullptr;
        BindSampler(dev, kFirstSurfaceStage + k, surface ? surface : m_flat, D3DTADDRESS_WRAP, D3DTEXF_LINEAR,
                    D3DTEXF_LINEAR);
        BindSampler(dev, kFirstFoamStateStage + k, foam ? foam : m_flat, D3DTADDRESS_WRAP, D3DTEXF_LINEAR,
                    D3DTEXF_NONE);
    }
    for (int slot = 0; slot < kWaterShadedMaskSlots; ++slot)
    {
        IDirect3DTexture9* mask = MaskTexture(preset.masks[slot]);
        BindSampler(dev, kFirstMaskStage + slot, mask ? mask : m_flat, D3DTADDRESS_WRAP, D3DTEXF_LINEAR,
                    D3DTEXF_LINEAR);
    }
}

void WaterRenderer::ShadeClasses(IDirect3DDevice9* dev, IDirect3DSurface9* target, IDirect3DSurface9* depthSurface,
                                 double seconds)
{
    const D3DVIEWPORT9& vp = m_in.viewport;
    const RECT world = ViewportRect(vp);
    dev->SetRenderTarget(0, target);
    dev->SetDepthStencilSurface(depthSurface);
    dev->SetViewport(&vp);
    dev->SetScissorRect(&world);
    dev->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
    dev->SetRenderState(D3DRS_COLORWRITEENABLE, kColourWriteRgb);
    dev->SetRenderState(D3DRS_STENCILENABLE, TRUE);
    dev->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
    dev->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
    dev->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
    dev->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
    dev->SetRenderState(D3DRS_STENCILMASK, kStencilAllBits);
    dev->SetRenderState(D3DRS_STENCILWRITEMASK, 0);
    m_shadingVariant = g_forcedShadingVariant >= 0 && g_forcedShadingVariant < kWaterQualityLevels
                           ? g_forcedShadingVariant
                           : ShadingVariant(m_cfg);
    dev->SetPixelShader(m_shade[m_shadingVariant]);
    dev->SetPixelShaderConstantF(0, &m_common[0][0], kCommonConstants);
    BindPointSampler(dev, kSceneColourStage, m_sceneColour, D3DTADDRESS_MIRROR);
    BindPointSampler(dev, kSceneDepthStage, m_sceneDepth, D3DTADDRESS_MIRROR);
    BindPointSampler(dev, kWaterDepthStage, m_waterDepth, D3DTADDRESS_CLAMP);
    const WaterData& data = GlobalWaterData();
    static_assert(sizeof(ShadingConstants) % sizeof(Float4) == 0, "water constants are whole registers");
    static_assert(kShadingFirstConstant + sizeof(ShadingConstants) / sizeof(Float4) <= kWaterPixelConstants,
                  "the water state block restores every constant the shading pass sets");
    for (int index = 1; index < kWaterClassCount; ++index)
    {
        const WaterClass waterClass = static_cast<WaterClass>(index);
        const WaterPreset* preset = m_draws[index] ? data.Preset(waterClass) : nullptr;
        if (!preset)
            continue;
        ShadingConstants constants;
        FillClassConstants(constants, *preset, waterClass, seconds);
        dev->SetPixelShaderConstantF(kShadingFirstConstant, &constants.light.x,
                                     sizeof(constants) / sizeof(Float4));
        BindClassTextures(dev, *preset);
        dev->SetRenderState(D3DRS_STENCILREF, static_cast<DWORD>(index));
        DrawFullscreen(dev);
        ++m_shadedClasses;
        m_shadedClassMask |= 1u << index;
    }
}

bool WaterRenderer::End(IDirect3DDevice9* dev, const SceneDepth& depth)
{
    m_shadedClasses = 0;
    m_shadedClassMask = 0;
    m_wavesSimulated = false;
    if (!m_armed)
        return Skip("water pass not armed");
    RestoreClientStencil(dev);
    m_armed = false;
    m_gpuTimer.Resume();
    const bool shaded = ShadeTaggedWater(dev, depth);
    m_gpuTimer.End();
    AddToSummary(shaded);
    LogSummaryWhenDue(dev);
    return shaded;
}

void WaterRenderer::AddToSummary(bool shaded)
{
    if (!shaded)
        return;
    m_summaryClasses |= m_shadedClassMask;
    if (!m_wavesSimulated)
        return;
    m_summaryWaveResolution = m_waveSettings.resolution;
    int tiles = 0;
    for (uint32_t mask = m_waveTiles; mask; mask &= mask - 1)
        ++tiles;
    m_summaryWaveTiles = std::max(m_summaryWaveTiles, tiles);
}

void WaterRenderer::LogSummaryWhenDue(IDirect3DDevice9* dev)
{
    if (!LogEnabled(LogLevel::Info) || !dev)
        return;
    const double now = QpcSeconds();
    if (m_summaryStart < 0.0)
        m_summaryStart = now;
    if (!g_summaryForced && now - m_summaryStart < kSummarySeconds)
        return;
    g_summaryForced = false;
    m_summaryStart = now;
    char gpu[kSummaryTextSize] = {};
    DescribeGpuTime(m_gpuTimer, dev, gpu, sizeof(gpu));
    char classes[kSummaryTextSize] = {};
    for (int index = 1; index < kWaterClassCount; ++index)
        if (m_summaryClasses & (1u << index))
        {
            const size_t used = std::strlen(classes);
            std::snprintf(classes + used, sizeof(classes) - used, "%s%s", used ? "+" : "",
                          WaterClassLabel(static_cast<WaterClass>(index)));
        }
    char waves[kSummaryTextSize] = "flat";
    if (m_summaryWaveResolution > 0)
        std::snprintf(waves, sizeof(waves), "%d (%d tiles)", m_summaryWaveResolution, m_summaryWaveTiles);
    VF_LOG_INFO("%s, classes %s, waves %s", gpu[0] ? gpu : "water gpu timing unavailable",
                classes[0] ? classes : "none", waves);
    m_summaryClasses = 0;
    m_summaryWaveResolution = 0;
    m_summaryWaveTiles = 0;
}

bool WaterRenderer::ShadeTaggedWater(IDirect3DDevice9* dev, const SceneDepth& depth)
{
    if (!AnyClassDrawn())
        return Skip("no water drawn");
    if (!dev || !depth.texture || !depth.bound || dev->TestCooperativeLevel() != D3D_OK || !m_state ||
        !m_sceneColour)
        return Skip("device not ready");
    SaveTargets(dev);
    D3DSURFACE_DESC depthDesc = {};
    if (!UsableTargets(depth.bound, m_in.viewport, depthDesc))
    {
        ReleaseTargets();
        return false;
    }
    const double seconds = WaterSeconds();
    const bool wavesPrepared = PrepareWaves(dev);
    BuildReflectionFog();
    CaptureClientState();
    for (DWORD i = 1; i < kMaxRenderTargets; ++i)
        dev->SetRenderTarget(i, nullptr);
    if (!depth.Refresh(dev))
    {
        RestoreTargets(dev);
        return Skip("multisampled depth copy failed");
    }
    dev->SetDepthStencilSurface(nullptr);
    SetPassState(dev);
    RaiseInjectedFault(WaterFaultStage::End);
    m_wavesSimulated = wavesPrepared && SimulateWaves(dev, seconds);
    dev->SetRenderTarget(1, nullptr);
    SetPassState(dev);
    CopyLinearDepth(dev, depth.texture, m_waterDepth);
    ShadeClasses(dev, m_saved.colour[0], depth.bound, seconds);
    RestoreTargets(dev);
    m_lastSeconds = seconds;
    LogWaveState();
    if (m_shadedClasses == 0)
        return Skip("no water preset for the drawn classes");
    if (!m_loggedFirstShade)
    {
        m_loggedFirstShade = true;
        VF_LOG_INFO("water shaded: %d class%s, waves %s, quality %d", m_shadedClasses,
                    m_shadedClasses == 1 ? "" : "es", m_wavesSimulated ? "simulated" : "flat",
                    QualityIndex(m_cfg) + 1);
    }
    return true;
}
