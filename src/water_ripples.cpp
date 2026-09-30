#include "water_ripples.h"

#include "fullscreen_triangle.h"
#include "log.h"

#include "ps_vw_ripple_step.h"
#include "vs_fullscreen.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace
{
constexpr D3DFORMAT kMapFormats[] = {D3DFMT_G16R16F, D3DFMT_A16B16G16R16F};
constexpr DWORD kAllChannels = 0xF;
constexpr DWORD kMaxRenderTargets = 4;
constexpr UINT kStepConstant = 0;
constexpr UINT kFirstSegmentConstant = 1;
constexpr UINT kFirstShapeConstant = kFirstSegmentConstant + kMaxWaterRippleDisturbances;
constexpr float kMaxWindowCentreYards = 100000.0f;
constexpr float kHeldFootprint = 1.0f;
constexpr float kAddedImpulse = 0.0f;
constexpr const char* kWithheldByHarness = "ripple support withheld by the harness";

struct RenderStateSetting
{
    D3DRENDERSTATETYPE state;
    DWORD value;
};

const RenderStateSetting kStepRenderStates[] = {
    {D3DRS_ZENABLE, D3DZB_FALSE},        {D3DRS_ZWRITEENABLE, FALSE},  {D3DRS_ALPHATESTENABLE, FALSE},
    {D3DRS_ALPHABLENDENABLE, FALSE},     {D3DRS_SEPARATEALPHABLENDENABLE, FALSE},
    {D3DRS_CULLMODE, D3DCULL_NONE},      {D3DRS_STENCILENABLE, FALSE}, {D3DRS_TWOSIDEDSTENCILMODE, FALSE},
    {D3DRS_SCISSORTESTENABLE, FALSE},    {D3DRS_COLORWRITEENABLE, kAllChannels},
    {D3DRS_SRGBWRITEENABLE, FALSE},      {D3DRS_FOGENABLE, FALSE},     {D3DRS_CLIPPLANEENABLE, 0},
    {D3DRS_FILLMODE, D3DFILL_SOLID},
};

struct SamplerSetting
{
    D3DSAMPLERSTATETYPE state;
    DWORD value;
};

const SamplerSetting kPointClampSampler[] = {
    {D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP}, {D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP},
    {D3DSAMP_MAGFILTER, D3DTEXF_POINT},    {D3DSAMP_MINFILTER, D3DTEXF_POINT},
    {D3DSAMP_MIPFILTER, D3DTEXF_NONE},     {D3DSAMP_SRGBTEXTURE, FALSE},
    {D3DSAMP_MAXMIPLEVEL, 0},              {D3DSAMP_MIPMAPLODBIAS, 0},
};

WaterRippleFault g_injectedRippleFault = WaterRippleFault::None;

template <typename T>
void SafeRelease(T*& p)
{
    if (p)
    {
        p->Release();
        p = nullptr;
    }
}

bool ShaderUnsupported(IDirect3DDevice9* dev, HRESULT result)
{
    return (result == D3DERR_INVALIDCALL || result == D3DERR_NOTAVAILABLE || result == E_INVALIDARG) &&
           dev->TestCooperativeLevel() == D3D_OK;
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

int WindowOrigin(float centre, int texels)
{
    return static_cast<int>(std::floor(static_cast<double>(centre) / kWaterRippleTexelYards)) - texels / 2;
}

bool UsableWindowCentre(const float centre[2])
{
    for (int axis = 0; axis < 2; ++axis)
        if (!(std::fabs(centre[axis]) <= kMaxWindowCentreYards))
            return false;
    return true;
}

bool OutsideWindow(const float from[2], const float to[2], float radius, int texels)
{
    for (int axis = 0; axis < 2; ++axis)
        if (std::max(from[axis], to[axis]) < -radius || std::min(from[axis], to[axis]) > texels + radius)
            return true;
    return false;
}
}

void InjectWaterRippleFault(WaterRippleFault fault)
{
    g_injectedRippleFault = fault;
}

WaterRipples::~WaterRipples()
{
    ReleaseAll();
}

void WaterRipples::ReleaseDefaultPool()
{
    SafeRelease(m_maps[0]);
    SafeRelease(m_maps[1]);
    m_texels = 0;
    Restart();
}

void WaterRipples::ReleaseAll()
{
    ReleaseDefaultPool();
    SafeRelease(m_step);
    SafeRelease(m_vs);
    SafeRelease(m_decl);
    m_device = nullptr;
    m_format = D3DFMT_UNKNOWN;
    m_capabilitiesChecked = false;
    m_unsupported = false;
    m_failure = "";
}

void WaterRipples::Restart()
{
    m_running = false;
    m_mapsCleared = false;
    m_quietSteps = 0;
    m_current = 0;
}

bool WaterRipples::Fail(const char* reason)
{
    m_failure = reason;
    return false;
}

bool WaterRipples::CheckCapabilities(IDirect3DDevice9* dev)
{
    m_capabilitiesChecked = true;
    D3DCAPS9 caps = {};
    D3DDEVICE_CREATION_PARAMETERS creation = {};
    IDirect3D9* d3d = nullptr;
    if (FAILED(dev->GetDeviceCaps(&caps)) || FAILED(dev->GetCreationParameters(&creation)) ||
        FAILED(dev->GetDirect3D(&d3d)) || !d3d)
    {
        m_capabilitiesChecked = false;
        return Fail("device capabilities unavailable");
    }
    D3DDISPLAYMODE mode = {};
    if (FAILED(dev->GetDisplayMode(0, &mode)))
        mode.Format = D3DFMT_X8R8G8B8;
    auto supports = [&](DWORD usage, D3DFORMAT format) {
        return SUCCEEDED(d3d->CheckDeviceFormat(creation.AdapterOrdinal, creation.DeviceType, mode.Format, usage,
                                                D3DRTYPE_TEXTURE, format));
    };
    for (D3DFORMAT format : kMapFormats)
        if (supports(D3DUSAGE_RENDERTARGET, format) && supports(D3DUSAGE_QUERY_FILTER, format))
        {
            m_format = format;
            break;
        }
    d3d->Release();
    const char* unsupported = nullptr;
    if (g_injectedRippleFault == WaterRippleFault::Unsupported)
        unsupported = kWithheldByHarness;
    else if (caps.PixelShaderVersion < D3DPS_VERSION(3, 0) || caps.VertexShaderVersion < D3DVS_VERSION(3, 0))
        unsupported = "shader model 3 unavailable";
    else if (m_format == D3DFMT_UNKNOWN)
        unsupported = "no filterable 16-bit floating-point render targets";
    else if (caps.MaxTextureWidth < static_cast<DWORD>(kWaterRippleTexels) ||
             caps.MaxTextureHeight < static_cast<DWORD>(kWaterRippleTexels))
        unsupported = "textures smaller than the ripple map";
    if (!unsupported)
        return true;
    m_unsupported = true;
    VF_LOG_INFO("water ripples unavailable: %s", unsupported);
    return Fail(unsupported);
}

bool WaterRipples::EnsureShaders(IDirect3DDevice9* dev)
{
    if (m_step)
        return true;
    static const D3DVERTEXELEMENT9 kElements[] = {
        {0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        D3DDECL_END(),
    };
    HRESULT result = dev->CreateVertexShader(reinterpret_cast<const DWORD*>(g_vs_fullscreen), &m_vs);
    if (SUCCEEDED(result))
        result = dev->CreateVertexDeclaration(kElements, &m_decl);
    if (SUCCEEDED(result))
        result = dev->CreatePixelShader(reinterpret_cast<const DWORD*>(g_ps_vw_ripple_step), &m_step);
    if (SUCCEEDED(result))
        return true;
    SafeRelease(m_step);
    SafeRelease(m_vs);
    SafeRelease(m_decl);
    VF_LOG_ERROR("water ripple shader creation failed: HRESULT 0x%08lX", static_cast<unsigned long>(result));
    if (!ShaderUnsupported(dev, result))
        return Fail("ripple shader creation failed");
    m_unsupported = true;
    return Fail("ripple shader unsupported");
}

bool WaterRipples::EnsureMaps(IDirect3DDevice9* dev, int texels)
{
    if (m_maps[0] && m_maps[1] && m_texels == texels)
        return true;
    ReleaseDefaultPool();
    const UINT side = static_cast<UINT>(texels);
    for (IDirect3DTexture9*& map : m_maps)
        if (g_injectedRippleFault == WaterRippleFault::MapCreation ||
            FAILED(dev->CreateTexture(side, side, 1, D3DUSAGE_RENDERTARGET, m_format, D3DPOOL_DEFAULT, &map,
                                      nullptr)))
        {
            map = nullptr;
            ReleaseDefaultPool();
            return Fail("ripple map creation failed");
        }
    m_texels = texels;
    return true;
}

bool WaterRipples::Supported(IDirect3DDevice9* dev)
{
    if (dev != m_device)
    {
        ReleaseAll();
        m_device = dev;
    }
    if (!m_unsupported && !m_capabilitiesChecked)
        CheckCapabilities(dev);
    return !m_unsupported;
}

bool WaterRipples::Prepare(IDirect3DDevice9* dev, int texels)
{
    if (!Supported(dev))
        return false;
    m_failure = "";
    if (texels != kWaterRippleTexels && texels != kWaterRippleTexelsLow)
        return Fail("ripple map size is not 256 or 512");
    return (m_capabilitiesChecked || CheckCapabilities(dev)) && EnsureShaders(dev) && EnsureMaps(dev, texels);
}

WaterRippleSchedule WaterRipples::Schedule(double seconds)
{
    WaterRippleSchedule schedule;
    if (!m_running || seconds < m_lastScheduled)
    {
        Restart();
        m_running = true;
        m_epoch = seconds;
        m_stepsDone = 0;
        m_lastScheduled = seconds;
        return schedule;
    }
    m_lastScheduled = seconds;
    const int64_t due =
        static_cast<int64_t>(std::floor((seconds - m_epoch) * kWaterRippleStepsPerSecond)) - m_stepsDone;
    if (due <= 0)
        return schedule;
    const int64_t dropped = std::max<int64_t>(0, due - kMaxWaterRippleStepsPerFrame);
    m_stepsDone += dropped;
    m_droppedSteps += static_cast<uint32_t>(dropped);
    schedule.dropped = static_cast<uint32_t>(dropped);
    schedule.steps = static_cast<int>(due - dropped);
    for (int i = 0; i < schedule.steps; ++i)
        schedule.stepSeconds[i] = m_epoch + static_cast<double>(m_stepsDone + 1 + i) / kWaterRippleStepsPerSecond;
    m_stepsDone += schedule.steps;
    return schedule;
}

void WaterRipples::ClearMaps(IDirect3DDevice9* dev)
{
    for (IDirect3DTexture9* map : m_maps)
    {
        SetTarget(dev, map);
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1.0f, 0);
    }
    m_mapsCleared = true;
}

void WaterRipples::SetStepState(IDirect3DDevice9* dev)
{
    for (const RenderStateSetting& setting : kStepRenderStates)
        dev->SetRenderState(setting.state, setting.value);
    for (DWORD i = 1; i < kMaxRenderTargets; ++i)
        dev->SetRenderTarget(i, nullptr);
    dev->SetDepthStencilSurface(nullptr);
    dev->SetVertexShader(m_vs);
    dev->SetVertexDeclaration(m_decl);
    dev->SetStreamSourceFreq(0, 1);
    dev->SetPixelShader(m_step);
    for (const SamplerSetting& setting : kPointClampSampler)
        dev->SetSamplerState(0, setting.state, setting.value);
}

WaterRipples::WindowShift WaterRipples::Recentre(IDirect3DDevice9* dev, const float centre[2])
{
    const bool usable = UsableWindowCentre(centre);
    const int desired[2] = {usable ? WindowOrigin(centre[0], m_texels) : m_origin[0],
                            usable ? WindowOrigin(centre[1], m_texels) : m_origin[1]};
    WindowShift shift = {desired[0] - m_origin[0], desired[1] - m_origin[1]};
    if (!m_mapsCleared || std::abs(shift.x) >= m_texels || std::abs(shift.y) >= m_texels)
    {
        ClearMaps(dev);
        shift = {0, 0};
    }
    m_origin[0] = desired[0];
    m_origin[1] = desired[1];
    return shift;
}

uint32_t WaterRipples::UploadDisturbances(IDirect3DDevice9* dev, WindowShift shift,
                                          const WaterRippleDisturbance* disturbances, uint32_t count)
{
    float segments[kMaxWaterRippleDisturbances][4] = {};
    float shapes[kMaxWaterRippleDisturbances][4] = {};
    uint32_t used = 0;
    for (uint32_t i = 0; i < count && used < kMaxWaterRippleDisturbances; ++i)
    {
        const WaterRippleDisturbance& d = disturbances[i];
        const float radius = d.radius / kWaterRippleTexelYards;
        if (!(radius > 0.0f) || !std::isfinite(d.amplitude) || (d.amplitude == 0.0f && !d.held))
            continue;
        float from[2];
        float to[2];
        for (int axis = 0; axis < 2; ++axis)
        {
            from[axis] = static_cast<float>(d.from[axis] / kWaterRippleTexelYards - m_origin[axis]);
            to[axis] = static_cast<float>(d.to[axis] / kWaterRippleTexelYards - m_origin[axis]);
        }
        if (OutsideWindow(from, to, radius, m_texels))
            continue;
        segments[used][0] = from[0];
        segments[used][1] = from[1];
        segments[used][2] = to[0];
        segments[used][3] = to[1];
        shapes[used][0] = 1.0f / radius;
        shapes[used][1] = d.amplitude;
        shapes[used][2] = d.held ? kHeldFootprint : kAddedImpulse;
        ++used;
    }
    const float step[4] = {static_cast<float>(shift.x), static_cast<float>(shift.y), 1.0f / m_texels,
                           static_cast<float>(used)};
    dev->SetPixelShaderConstantF(kStepConstant, step, 1);
    if (used)
    {
        dev->SetPixelShaderConstantF(kFirstSegmentConstant, &segments[0][0], used);
        dev->SetPixelShaderConstantF(kFirstShapeConstant, &shapes[0][0], used);
    }
    return used;
}

void WaterRipples::Step(IDirect3DDevice9* dev, const float centre[2], const WaterRippleDisturbance* disturbances,
                        uint32_t count)
{
    if (!m_maps[0] || !m_maps[1] || !m_step)
        return;
    SetStepState(dev);
    const WindowShift shift = Recentre(dev, centre);
    const uint32_t used = UploadDisturbances(dev, shift, disturbances, count);
    SetTarget(dev, m_maps[1 - m_current]);
    dev->SetTexture(0, m_maps[m_current]);
    DrawFullscreenTriangle(dev);
    dev->SetTexture(0, nullptr);
    m_current = 1 - m_current;
    ++m_stepsRun;
    m_quietSteps = used ? 0 : m_quietSteps + 1;
    if (m_quietSteps >= kWaterRippleQuietSteps)
        Restart();
}

WaterRippleWindow WaterRipples::Window(double seconds) const
{
    WaterRippleWindow window;
    window.origin[0] = static_cast<float>(m_origin[0] * static_cast<double>(kWaterRippleTexelYards));
    window.origin[1] = static_cast<float>(m_origin[1] * static_cast<double>(kWaterRippleTexelYards));
    window.extent = m_texels * kWaterRippleTexelYards;
    window.texels = m_texels;
    const double progress = (seconds - m_epoch) * kWaterRippleStepsPerSecond - static_cast<double>(m_stepsDone);
    window.weight = static_cast<float>(std::clamp(progress, 0.0, 1.0));
    return window;
}
