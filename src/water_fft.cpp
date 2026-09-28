#include "water_fft.h"

#include "log.h"
#include "water_spectrum.h"

#include "ps_vw_fft_assemble_fused.h"
#include "ps_vw_fft_columns.h"
#include "ps_vw_fft_columns_fused.h"
#include "ps_vw_fft_evolve.h"
#include "ps_vw_fft_foam.h"
#include "ps_vw_fft_mip.h"
#include "ps_vw_fft_mip_pair.h"
#include "ps_vw_fft_rows.h"
#include "ps_vw_fft_rows_fused.h"
#include "ps_vw_fft_surface.h"
#include "ps_vw_fft_surface_foam.h"
#include "vs_fullscreen.h"

#include <algorithm>
#include <cstring>

namespace
{
constexpr D3DFORMAT kSpectrumFormat = D3DFMT_A32B32G32R32F;
constexpr D3DFORMAT kMapFormat = D3DFMT_A16B16G16R16F;
constexpr DWORD kAllChannels = 0xF;
constexpr UINT kLargestMapSide = kWaterFftHighResolution;
constexpr float kLargestFoamStepSeconds = 0.1f;
constexpr unsigned kFramesBetweenCreationAttempts = 120;
constexpr DWORD kFirstUnusedStage = 2;
constexpr float kFullscreenTriangle[3][4] = {
    {-1.0f, -1.0f, 0.0f, 1.0f}, {-1.0f, 3.0f, 0.0f, 1.0f}, {3.0f, -1.0f, 0.0f, 1.0f}};

struct Float4
{
    float x, y, z, w;
};

struct SamplerSetting
{
    D3DSAMPLERSTATETYPE state;
    DWORD value;
};

const SamplerSetting kPointClampSampler[] = {
    {D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP}, {D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP},
    {D3DSAMP_MAGFILTER, D3DTEXF_POINT},    {D3DSAMP_MINFILTER, D3DTEXF_POINT},
    {D3DSAMP_MIPFILTER, D3DTEXF_POINT},    {D3DSAMP_SRGBTEXTURE, FALSE},
    {D3DSAMP_MAXMIPLEVEL, 0},              {D3DSAMP_MIPMAPLODBIAS, 0},
};

template <typename T>
void SafeRelease(T*& p)
{
    if (p)
    {
        p->Release();
        p = nullptr;
    }
}

bool CreateTarget(IDirect3DDevice9* dev, UINT width, UINT height, UINT levels, D3DFORMAT format,
                  IDirect3DTexture9** out)
{
    return SUCCEEDED(
        dev->CreateTexture(width, height, levels, D3DUSAGE_RENDERTARGET, format, D3DPOOL_DEFAULT, out, nullptr));
}

bool UploadTexels(IDirect3DDevice9* dev, UINT width, UINT height, const std::vector<float>& texels,
                  IDirect3DTexture9** out)
{
    IDirect3DTexture9* staging = nullptr;
    if (FAILED(dev->CreateTexture(width, height, 1, 0, kSpectrumFormat, D3DPOOL_SYSTEMMEM, &staging, nullptr)))
        return false;
    D3DLOCKED_RECT locked = {};
    bool uploaded = SUCCEEDED(staging->LockRect(0, &locked, nullptr, 0));
    if (uploaded)
    {
        const size_t rowFloats = static_cast<size_t>(width) * kWaterFftTexelChannels;
        for (UINT row = 0; row < height; ++row)
            std::memcpy(static_cast<BYTE*>(locked.pBits) + static_cast<size_t>(row) * locked.Pitch,
                        &texels[row * rowFloats], rowFloats * sizeof(float));
        uploaded = SUCCEEDED(staging->UnlockRect(0));
    }
    uploaded = uploaded &&
               SUCCEEDED(dev->CreateTexture(width, height, 1, 0, kSpectrumFormat, D3DPOOL_DEFAULT, out, nullptr)) &&
               SUCCEEDED(dev->UpdateTexture(staging, *out));
    staging->Release();
    if (!uploaded)
        SafeRelease(*out);
    return uploaded;
}

bool ShaderUnsupported(IDirect3DDevice9* dev, HRESULT result)
{
    return (result == D3DERR_INVALIDCALL || result == D3DERR_NOTAVAILABLE || result == E_INVALIDARG) &&
           dev->TestCooperativeLevel() == D3D_OK;
}

int LastStageOfFirstPass(int stages)
{
    return 1 - stages % 2;
}

float FoamStepSeconds(float deltaSeconds)
{
    return deltaSeconds > 0.0f ? std::min(deltaSeconds, kLargestFoamStepSeconds) : 0.0f;
}
}

WaterFftPasses::~WaterFftPasses()
{
    ReleaseAll();
}

void WaterFftPasses::ReleaseResolutionResources()
{
    SafeRelease(m_noise);
    SafeRelease(m_butterflies);
    SafeRelease(m_atlas[0]);
    SafeRelease(m_atlas[1]);
    SafeRelease(m_mapScratch[0]);
    SafeRelease(m_mapScratch[1]);
    m_slots = 0;
    m_displacementAtlas = 0;
}

void WaterFftPasses::ReleaseDefaultPool()
{
    ReleaseResolutionResources();
}

void WaterFftPasses::ReleaseAll()
{
    ReleaseDefaultPool();
    for (IDirect3DPixelShader9*& shader : m_shaders)
        SafeRelease(shader);
    SafeRelease(m_vs);
    SafeRelease(m_decl);
    m_device = nullptr;
    m_failure = "";
    m_unsupported = false;
    m_capabilitiesChecked = false;
    m_resolution = 0;
    m_tableResolution = 0;
    m_noiseTexels.clear();
    m_butterflyTexels.clear();
}

bool WaterFftPasses::Fail(const char* reason)
{
    m_failure = reason;
    return false;
}

bool WaterFftPasses::Unsupport(const char* reason)
{
    m_unsupported = true;
    VF_LOG_INFO("water waves unavailable: %s", reason);
    return Fail(reason);
}

int WaterFftPasses::MaxSlots() const
{
    const UINT side = m_resolution > 0 ? static_cast<UINT>(m_resolution) : kLargestMapSide;
    return std::max(1, std::min(kWaterMaxTiles, static_cast<int>(m_maxTextureWidth / side)));
}

bool WaterFftPasses::CheckCapabilities(IDirect3DDevice9* dev)
{
    D3DCAPS9 caps = {};
    D3DDEVICE_CREATION_PARAMETERS creation = {};
    IDirect3D9* d3d = nullptr;
    if (FAILED(dev->GetDeviceCaps(&caps)) || FAILED(dev->GetCreationParameters(&creation)) ||
        FAILED(dev->GetDirect3D(&d3d)) || !d3d)
        return Fail("device capabilities unavailable");
    D3DDISPLAYMODE mode = {};
    if (FAILED(dev->GetDisplayMode(0, &mode)))
        mode.Format = D3DFMT_X8R8G8B8;
    auto supports = [&](DWORD usage, D3DFORMAT format) {
        return SUCCEEDED(d3d->CheckDeviceFormat(creation.AdapterOrdinal, creation.DeviceType, mode.Format, usage,
                                                D3DRTYPE_TEXTURE, format));
    };
    const bool spectrumTextures = supports(0, kSpectrumFormat) && supports(D3DUSAGE_RENDERTARGET, kSpectrumFormat);
    const bool mapTargets = supports(D3DUSAGE_RENDERTARGET, kMapFormat);
    const bool mapFiltering = supports(D3DUSAGE_QUERY_FILTER, kMapFormat);
    d3d->Release();
    m_capabilitiesChecked = true;
    if (caps.PixelShaderVersion < D3DPS_VERSION(3, 0) || caps.VertexShaderVersion < D3DVS_VERSION(3, 0))
        return Unsupport("shader model 3 unavailable");
    if (!spectrumTextures)
        return Unsupport("no 32-bit floating-point render targets");
    if (!mapTargets)
        return Unsupport("no 16-bit floating-point render targets");
    if (!mapFiltering)
        return Unsupport("16-bit floating-point textures cannot be filtered");
    if (caps.MaxTextureWidth < kLargestMapSide || caps.MaxTextureHeight < kLargestMapSide)
        return Unsupport("textures smaller than the wave maps");
    m_maxTextureWidth = caps.MaxTextureWidth;
    m_multipleTargets = caps.NumSimultaneousRTs >= 2;
    m_independentWriteMasks = (caps.PrimitiveMiscCaps & D3DPMISCCAPS_INDEPENDENTWRITEMASKS) != 0;
    return true;
}

bool WaterFftPasses::EnsureShaders(IDirect3DDevice9* dev)
{
    if (m_vs)
        return true;
    struct PixelShaderRequest
    {
        PassShader shader;
        const BYTE* code;
    };
    const PixelShaderRequest requests[] = {
        {PassShader::Evolve, g_ps_vw_fft_evolve},
        {PassShader::Rows, g_ps_vw_fft_rows},
        {PassShader::RowsFused, g_ps_vw_fft_rows_fused},
        {PassShader::Columns, g_ps_vw_fft_columns},
        {PassShader::ColumnsFused, g_ps_vw_fft_columns_fused},
        {PassShader::AssembleFused, g_ps_vw_fft_assemble_fused},
        {PassShader::SurfaceAndFoam, g_ps_vw_fft_surface_foam},
        {PassShader::Surface, g_ps_vw_fft_surface},
        {PassShader::Foam, g_ps_vw_fft_foam},
        {PassShader::Mip, g_ps_vw_fft_mip},
        {PassShader::MipPair, g_ps_vw_fft_mip_pair},
    };
    static const D3DVERTEXELEMENT9 kElements[] = {
        {0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        D3DDECL_END(),
    };
    HRESULT result = dev->CreateVertexShader(reinterpret_cast<const DWORD*>(g_vs_fullscreen), &m_vs);
    for (const PixelShaderRequest& request : requests)
        if (SUCCEEDED(result))
            result = dev->CreatePixelShader(reinterpret_cast<const DWORD*>(request.code),
                                            &m_shaders[static_cast<int>(request.shader)]);
    if (SUCCEEDED(result))
        result = dev->CreateVertexDeclaration(kElements, &m_decl);
    if (SUCCEEDED(result))
        return true;
    for (IDirect3DPixelShader9*& shader : m_shaders)
        SafeRelease(shader);
    SafeRelease(m_vs);
    SafeRelease(m_decl);
    VF_LOG_ERROR("water wave shader creation failed: HRESULT 0x%08lX", static_cast<unsigned long>(result));
    return ShaderUnsupported(dev, result) ? Unsupport("wave shaders unsupported") : Fail("wave shader creation failed");
}

bool WaterFftPasses::EnsureTables(IDirect3DDevice9* dev)
{
    if (m_tableResolution != m_resolution)
    {
        m_noiseTexels = WaterSpectrumNoise(m_resolution);
        m_butterflyTexels = WaterButterflyTexels(m_resolution);
        m_tableResolution = m_resolution;
    }
    const UINT side = static_cast<UINT>(m_resolution);
    if (!m_noise && !UploadTexels(dev, side, side, m_noiseTexels, &m_noise))
        return Fail("wave noise upload failed");
    if (!m_butterflies &&
        !UploadTexels(dev, static_cast<UINT>(WaterFftStages(m_resolution)), side, m_butterflyTexels, &m_butterflies))
        return Fail("wave butterfly table upload failed");
    return true;
}

bool WaterFftPasses::EnsureAtlas(IDirect3DDevice9* dev, int slots)
{
    if (m_atlas[0] && m_atlas[1] && m_slots >= slots)
        return true;
    SafeRelease(m_atlas[0]);
    SafeRelease(m_atlas[1]);
    m_slots = 0;
    const UINT side = static_cast<UINT>(m_resolution);
    if (!CreateTarget(dev, side * slots, side, 1, kSpectrumFormat, &m_atlas[0]) ||
        !CreateTarget(dev, side * slots, side, 1, kSpectrumFormat, &m_atlas[1]))
    {
        SafeRelease(m_atlas[0]);
        SafeRelease(m_atlas[1]);
        return Fail("wave spectrum atlas creation failed");
    }
    m_slots = slots;
    return true;
}

bool WaterFftPasses::EnsureMapScratch()
{
    for (IDirect3DTexture9*& scratch : m_mapScratch)
        if (!scratch && !CreateSurfaceMap(&scratch))
            return Fail("wave map scratch creation failed");
    return true;
}

bool WaterFftPasses::Prepare(IDirect3DDevice9* dev, int resolution, int slots)
{
    if (dev != m_device)
    {
        ReleaseAll();
        m_device = dev;
    }
    if (m_unsupported)
        return false;
    m_failure = "";
    if (!IsWaterFftResolution(resolution))
        return Fail("wave resolution is not 128 or 256");
    if (!m_capabilitiesChecked && !CheckCapabilities(dev))
        return false;
    if (resolution != m_resolution)
    {
        ReleaseResolutionResources();
        m_resolution = resolution;
    }
    return EnsureShaders(dev) && EnsureTables(dev) && EnsureAtlas(dev, std::clamp(slots, 1, MaxSlots())) &&
           EnsureMapScratch();
}

bool WaterFftPasses::CreateSurfaceMap(IDirect3DTexture9** surface) const
{
    const UINT side = static_cast<UINT>(m_resolution);
    return m_device && m_resolution > 0 &&
           CreateTarget(m_device, side, side, static_cast<UINT>(WaterFftStages(m_resolution) + 1), kMapFormat,
                        surface);
}

bool WaterFftPasses::CreateFoamMap(IDirect3DTexture9** foam) const
{
    const UINT side = static_cast<UINT>(m_resolution);
    return m_device && m_resolution > 0 && CreateTarget(m_device, side, side, 1, kMapFormat, foam);
}

void WaterFftPasses::Begin(IDirect3DDevice9* dev)
{
    m_passDevice = dev;
    m_draws = 0;
    m_boundShader = nullptr;
    for (int index = 0; index < 2; ++index)
    {
        m_boundTargets[index] = nullptr;
        m_targetKnown[index] = false;
    }
    for (int stage = 0; stage < kSamplerStages; ++stage)
    {
        m_boundTextures[stage] = nullptr;
        m_samplerKnown[stage] = false;
    }
    dev->GetScissorRect(&m_callerScissor);
    dev->SetVertexShader(m_vs);
    dev->SetVertexDeclaration(m_decl);
    for (DWORD stage = kFirstUnusedStage; stage < kSamplerStages; ++stage)
        dev->SetTexture(stage, nullptr);
    DWORD secondTargetWrites = kAllChannels;
    if (m_independentWriteMasks)
        dev->GetRenderState(D3DRS_COLORWRITEENABLE1, &secondTargetWrites);
    m_pairTargets = m_multipleTargets && (secondTargetWrites & kAllChannels) == kAllChannels;
    BindTarget(1, nullptr, 0);
}

void WaterFftPasses::End()
{
    BindTarget(1, nullptr, 0);
    m_passDevice->SetScissorRect(&m_callerScissor);
    m_passDevice = nullptr;
}

void WaterFftPasses::UseShader(PassShader shader)
{
    IDirect3DPixelShader9* const pixelShader = m_shaders[static_cast<int>(shader)];
    if (pixelShader == m_boundShader)
        return;
    m_passDevice->SetPixelShader(pixelShader);
    m_boundShader = pixelShader;
}

void WaterFftPasses::BindTexture(DWORD stage, IDirect3DBaseTexture9* texture)
{
    if (!m_samplerKnown[stage])
    {
        for (const SamplerSetting& setting : kPointClampSampler)
            m_passDevice->SetSamplerState(stage, setting.state, setting.value);
        m_passDevice->SetTexture(stage, texture);
        m_boundTextures[stage] = texture;
        m_samplerKnown[stage] = true;
        return;
    }
    if (m_boundTextures[stage] == texture)
        return;
    m_passDevice->SetTexture(stage, texture);
    m_boundTextures[stage] = texture;
}

void WaterFftPasses::BindTarget(DWORD index, IDirect3DTexture9* texture, UINT level)
{
    IDirect3DSurface9* surface = nullptr;
    if (texture && FAILED(texture->GetSurfaceLevel(level, &surface)))
        return;
    if (!m_targetKnown[index] || m_boundTargets[index] != surface)
    {
        m_passDevice->SetRenderTarget(index, surface);
        m_boundTargets[index] = surface;
        m_targetKnown[index] = true;
    }
    SafeRelease(surface);
}

void WaterFftPasses::BindTargets(IDirect3DTexture9* first, IDirect3DTexture9* second, UINT level)
{
    BindTarget(1, nullptr, 0);
    BindTarget(0, first, level);
    BindTarget(1, second, level);
}

void WaterFftPasses::SetViewport(UINT x, UINT width, UINT height)
{
    const D3DVIEWPORT9 viewport = {x, 0, width, height, 0.0f, 1.0f};
    m_passDevice->SetViewport(&viewport);
}

void WaterFftPasses::Draw()
{
    m_passDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, kFullscreenTriangle, sizeof(kFullscreenTriangle[0]));
    ++m_draws;
}

void WaterFftPasses::ClearFoam(IDirect3DTexture9* foam)
{
    const UINT side = static_cast<UINT>(m_resolution);
    BindTargets(foam, nullptr, 0);
    SetViewport(0, side, side);
    m_passDevice->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1.0f, 0);
}

void WaterFftPasses::Evolve(int slot, const WaterFftTile& tile, const WaterFftSettings& settings, double seconds)
{
    const WaterSpectrumShape shape = MakeWaterSpectrumShape(tile, settings.windSpeed, settings.windDirection);
    const WaterLoopClock clock = MakeWaterLoopClock(seconds);
    const float side = static_cast<float>(m_resolution);
    const Float4 constants[4] = {
        {side, 1.0f / side, slot * side, 0.5f * side},
        {static_cast<float>(kWaterTwoPi / shape.length), static_cast<float>(shape.amplitude),
         static_cast<float>(shape.alignment), static_cast<float>(shape.inversePeakLengthSquared)},
        {static_cast<float>(shape.wind[0]), static_cast<float>(shape.wind[1]), 0.0f, 0.0f},
        {clock.coarse, clock.fine, WaterLoopFrequenciesPerRootRadius(shape.length), 0.0f},
    };
    BindTargets(Spectrum(), nullptr, 0);
    SetViewport(static_cast<UINT>(slot * m_resolution), static_cast<UINT>(m_resolution),
                static_cast<UINT>(m_resolution));
    UseShader(PassShader::Evolve);
    BindTexture(0, m_noise);
    m_passDevice->SetPixelShaderConstantF(0, &constants[0].x, 4);
    Draw();
}

WaterFftPasses::PassShader WaterFftPasses::TransformShader(bool columns, bool fused, bool assemble)
{
    if (assemble)
        return PassShader::AssembleFused;
    if (columns)
        return fused ? PassShader::ColumnsFused : PassShader::Columns;
    return fused ? PassShader::RowsFused : PassShader::Rows;
}

void WaterFftPasses::Transform(int slots, int referenceResolution)
{
    const int stages = WaterFftStages(m_resolution);
    const float side = static_cast<float>(m_resolution);
    const Float4 grid = {side, 1.0f / side, static_cast<float>(m_slots), 1.0f / m_slots};
    const float inverseReferenceArea =
        static_cast<float>(1.0 / (static_cast<double>(referenceResolution) * referenceResolution));
    const UINT width = static_cast<UINT>(std::clamp(slots, 1, m_slots) * m_resolution);
    auto stageCoordinate = [stages](int stage) { return (stage + 0.5f) / stages; };
    m_passDevice->SetPixelShaderConstantF(0, &grid.x, 1);
    BindTexture(1, m_butterflies);
    int source = 0;
    for (const bool columns : {false, true})
        for (int lastStage = LastStageOfFirstPass(stages); lastStage < stages; lastStage += 2)
        {
            const bool fused = lastStage > 0;
            const Float4 stage = {stageCoordinate(lastStage), inverseReferenceArea,
                                  stageCoordinate(std::max(lastStage - 1, 0)), 0.0f};
            BindTexture(0, m_atlas[source]);
            BindTargets(m_atlas[1 - source], nullptr, 0);
            SetViewport(0, width, static_cast<UINT>(m_resolution));
            UseShader(TransformShader(columns, fused, columns && lastStage == stages - 1));
            m_passDevice->SetPixelShaderConstantF(1, &stage.x, 1);
            Draw();
            source = 1 - source;
        }
    m_displacementAtlas = source;
}

void WaterFftPasses::UpdateMaps(int slot, const WaterFftTile& tile, float deltaSeconds, IDirect3DTexture9* surface,
                                IDirect3DTexture9* previousFoam, IDirect3DTexture9* nextFoam)
{
    const WaterSurfaceStencil stencil = MakeWaterSurfaceStencil(m_resolution, WaterTileLength(tile));
    const float side = static_cast<float>(m_resolution);
    const Float4 constants[4] = {
        {side, 1.0f / side, slot * side, 1.0f / (m_slots * side)},
        {static_cast<float>(stencil.halfTexelsPerUnit), static_cast<float>(stencil.wideRadius),
         static_cast<float>(stencil.inverseWideBaseline), deltaSeconds},
        {tile.foam[0], tile.foam[1], tile.foam[2], 0.0f},
        {tile.oxygen[0], tile.oxygen[1], tile.oxygen[2], 0.0f},
    };
    BindTexture(0, Displacement());
    BindTexture(1, previousFoam);
    m_passDevice->SetPixelShaderConstantF(0, &constants[0].x, 4);
    if (m_pairTargets)
        UpdatePairedMaps(surface, nextFoam);
    else
        UpdateSeparateMaps(surface, nextFoam);
}

void WaterFftPasses::UpdatePairedMaps(IDirect3DTexture9* surface, IDirect3DTexture9* nextFoam)
{
    const UINT side = static_cast<UINT>(m_resolution);
    BindTargets(m_mapScratch[0], nextFoam, 0);
    SetViewport(0, side, side);
    UseShader(PassShader::SurfaceAndFoam);
    Draw();
    ResampleLevel(PassShader::Mip, m_mapScratch[0], 0, surface, nullptr, 0);
    const UINT stages = static_cast<UINT>(WaterFftStages(m_resolution));
    for (UINT level = 1; level <= stages; ++level)
        ResampleLevel(PassShader::MipPair, m_mapScratch[(level - 1) % 2], level - 1, surface, m_mapScratch[level % 2],
                      level);
}

void WaterFftPasses::UpdateSeparateMaps(IDirect3DTexture9* surface, IDirect3DTexture9* nextFoam)
{
    const UINT side = static_cast<UINT>(m_resolution);
    BindTargets(surface, nullptr, 0);
    SetViewport(0, side, side);
    UseShader(PassShader::Surface);
    Draw();
    BindTargets(nextFoam, nullptr, 0);
    SetViewport(0, side, side);
    UseShader(PassShader::Foam);
    Draw();
    const UINT stages = static_cast<UINT>(WaterFftStages(m_resolution));
    for (UINT level = 1; level <= stages; ++level)
    {
        ResampleLevel(PassShader::Mip, surface, level - 1, m_mapScratch[0], nullptr, level);
        ResampleLevel(PassShader::Mip, m_mapScratch[0], level, surface, nullptr, level);
    }
}

void WaterFftPasses::ResampleLevel(PassShader shader, IDirect3DTexture9* source, UINT sourceLevel,
                                   IDirect3DTexture9* target, IDirect3DTexture9* pairedTarget, UINT level)
{
    const UINT size = static_cast<UINT>(m_resolution) >> level;
    const bool downsample = sourceLevel < level;
    const Float4 resample = {static_cast<float>(sourceLevel), 1.0f / (downsample ? 2 * size : size),
                             downsample ? 2.0f : 1.0f, downsample ? 1.0f : 0.0f};
    BindTexture(0, source);
    BindTargets(target, pairedTarget, level);
    SetViewport(0, size, size);
    UseShader(shader);
    m_passDevice->SetPixelShaderConstantF(0, &resample.x, 1);
    Draw();
}

WaterFft::~WaterFft()
{
    ReleaseAll();
}

void WaterFft::ReleaseTileMaps(TileMaps& maps)
{
    SafeRelease(maps.surface);
    SafeRelease(maps.foam[0]);
    SafeRelease(maps.foam[1]);
    maps.latestFoam = 0;
    maps.simulated = false;
}

void WaterFft::ReleaseDefaultPool()
{
    m_passes.ReleaseDefaultPool();
    for (TileMaps& maps : m_tiles)
        ReleaseTileMaps(maps);
    m_mapResolution = 0;
    m_framesUntilRetry = 0;
}

void WaterFft::ReleaseAll()
{
    ReleaseDefaultPool();
    m_passes.ReleaseAll();
    m_tiles.clear();
}

bool WaterFft::Fail(const char* reason)
{
    m_failure = reason;
    return false;
}

bool WaterFft::FailCreation(const char* reason)
{
    VF_LOG_ERROR("water waves: %s; retrying in %u frames", reason, kFramesBetweenCreationAttempts);
    ReleaseDefaultPool();
    m_retryFailure = reason;
    m_framesUntilRetry = kFramesBetweenCreationAttempts;
    return Fail(reason);
}

void WaterFft::ForgetChangedTiles(const std::vector<WaterFftTile>& tiles)
{
    for (size_t i = tiles.size(); i < m_tiles.size(); ++i)
        ReleaseTileMaps(m_tiles[i]);
    m_tiles.resize(tiles.size());
    for (size_t i = 0; i < tiles.size(); ++i)
        if (std::memcmp(&m_tiles[i].params, &tiles[i], sizeof(WaterFftTile)) != 0)
        {
            ReleaseTileMaps(m_tiles[i]);
            m_tiles[i].params = tiles[i];
        }
}

bool WaterFft::EnsureTileMaps(TileMaps& maps, bool& created)
{
    created = false;
    if (maps.surface && maps.foam[0] && maps.foam[1])
        return true;
    ReleaseTileMaps(maps);
    if (!m_passes.CreateSurfaceMap(&maps.surface) || !m_passes.CreateFoamMap(&maps.foam[0]) ||
        !m_passes.CreateFoamMap(&maps.foam[1]))
    {
        ReleaseTileMaps(maps);
        return false;
    }
    created = true;
    return true;
}

void WaterFft::SimulateBatch(const std::vector<WaterFftTile>& tiles, const int* indices, int count,
                             const WaterFftSettings& settings, double seconds, float deltaSeconds)
{
    for (int slot = 0; slot < count; ++slot)
        m_passes.Evolve(slot, tiles[indices[slot]], settings, seconds);
    m_passes.Transform(count, settings.referenceResolution);
    for (int slot = 0; slot < count; ++slot)
    {
        TileMaps& maps = m_tiles[indices[slot]];
        const int next = 1 - maps.latestFoam;
        m_passes.UpdateMaps(slot, tiles[indices[slot]], deltaSeconds, maps.surface, maps.foam[maps.latestFoam],
                            maps.foam[next]);
        maps.latestFoam = next;
        maps.simulated = true;
    }
}

void WaterFft::LogPlan(int resolution, int tiles)
{
    if (resolution == m_loggedResolution && tiles == m_loggedTiles && m_passes.Draws() == m_loggedDraws)
        return;
    m_loggedResolution = resolution;
    m_loggedTiles = tiles;
    m_loggedDraws = m_passes.Draws();
    VF_LOG_INFO("water waves: %dx%d, %d tiles, %u draws per frame%s", resolution, resolution, tiles,
                m_passes.Draws(), m_passes.PairsTargets() ? "" : ", maps written in separate passes");
}

bool WaterFft::Simulate(IDirect3DDevice9* dev, const WaterFftSettings& settings, const std::vector<WaterFftTile>& tiles,
                        uint32_t tileMask, double seconds, float deltaSeconds)
{
    m_failure = "";
    if (!dev)
        return Fail("no device");
    if (!IsWaterFftResolution(settings.resolution))
        return Fail("wave resolution is not 128 or 256");
    if (settings.referenceResolution <= 0)
        return Fail("wave reference resolution is not positive");
    if (tiles.size() > static_cast<size_t>(kWaterMaxTiles))
        return Fail("more wave tiles than the simulation holds");
    if (dev->TestCooperativeLevel() != D3D_OK)
        return Fail("device not ready");
    if (m_framesUntilRetry > 0)
    {
        --m_framesUntilRetry;
        return Fail(m_retryFailure);
    }
    ForgetChangedTiles(tiles);
    int active[kWaterMaxTiles] = {};
    int count = 0;
    for (size_t i = 0; i < tiles.size(); ++i)
        if (tileMask & (1u << i))
            active[count++] = static_cast<int>(i);
    if (count == 0)
        return true;
    if (!m_passes.Prepare(dev, settings.resolution, count))
        return m_passes.Unsupported() ? Fail(m_passes.LastFailure()) : FailCreation(m_passes.LastFailure());
    if (m_mapResolution != settings.resolution)
    {
        for (TileMaps& maps : m_tiles)
            ReleaseTileMaps(maps);
        m_mapResolution = settings.resolution;
    }
    bool created[kWaterMaxTiles] = {};
    for (int slot = 0; slot < count; ++slot)
        if (!EnsureTileMaps(m_tiles[active[slot]], created[slot]))
            return FailCreation("wave map creation failed");
    m_passes.Begin(dev);
    for (int slot = 0; slot < count; ++slot)
        if (created[slot])
            for (IDirect3DTexture9* foam : m_tiles[active[slot]].foam)
                m_passes.ClearFoam(foam);
    const float foamStep = FoamStepSeconds(deltaSeconds);
    const int batch = m_passes.MaxSlots();
    for (int first = 0; first < count; first += batch)
        SimulateBatch(tiles, active + first, std::min(batch, count - first), settings, seconds, foamStep);
    m_passes.End();
    LogPlan(settings.resolution, count);
    return true;
}

IDirect3DTexture9* WaterFft::Surface(int tile) const
{
    if (tile < 0 || static_cast<size_t>(tile) >= m_tiles.size() || !m_tiles[tile].simulated)
        return nullptr;
    return m_tiles[tile].surface;
}

IDirect3DTexture9* WaterFft::Foam(int tile) const
{
    if (tile < 0 || static_cast<size_t>(tile) >= m_tiles.size() || !m_tiles[tile].simulated)
        return nullptr;
    return m_tiles[tile].foam[m_tiles[tile].latestFoam];
}
