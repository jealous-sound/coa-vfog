#include "material_fog.h"

#include "log.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
constexpr size_t kMaterialPairCacheLimit = 512;
constexpr unsigned kMaterialFallbackLogLimit = 12;
constexpr UINT kMaximumMaterialShaderBytes = 65536;
constexpr D3DSAMPLERSTATETYPE kAtlasSamplerStates[] = {
    D3DSAMP_ADDRESSU, D3DSAMP_ADDRESSV, D3DSAMP_ADDRESSW, D3DSAMP_MINFILTER, D3DSAMP_MAGFILTER,
    D3DSAMP_MIPFILTER, D3DSAMP_SRGBTEXTURE, D3DSAMP_MAXMIPLEVEL,
};
constexpr DWORD kAtlasSamplerValues[] = {
    D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP, D3DTEXF_LINEAR, D3DTEXF_LINEAR,
    D3DTEXF_NONE, FALSE, 0,
};
constexpr size_t kAtlasSamplerStateCount = sizeof(kAtlasSamplerStates) / sizeof(kAtlasSamplerStates[0]);

template <class T> void ReleasePointer(T*& object)
{
    if (object)
        object->Release();
    object = nullptr;
}

template <class T> bool ReadShader(T* shader, std::vector<DWORD>& code)
{
    UINT bytes = 0;
    if (!shader || FAILED(shader->GetFunction(nullptr, &bytes)) || bytes < 8 ||
        bytes > kMaximumMaterialShaderBytes || bytes % sizeof(DWORD) != 0)
        return false;
    code.resize(bytes / sizeof(DWORD));
    return SUCCEEDED(shader->GetFunction(code.data(), &bytes));
}
}

struct MaterialFog::ShaderPair
{
    IDirect3DVertexShader9* originalVertex = nullptr;
    IDirect3DPixelShader9* originalPixel = nullptr;
    IDirect3DVertexShader9* fogVertex = nullptr;
    IDirect3DPixelShader9* fogPixel = nullptr;
    FogMaterialResources resources;
    unsigned long long used = 0;
    bool ready = false;

    ~ShaderPair()
    {
        ReleasePointer(originalVertex);
        ReleasePointer(originalPixel);
        ReleasePointer(fogVertex);
        ReleasePointer(fogPixel);
    }
};

struct MaterialFog::SavedDraw
{
    IDirect3DDevice9* device = nullptr;
    IDirect3DVertexShader9* vertex = nullptr;
    IDirect3DPixelShader9* pixel = nullptr;
    IDirect3DBaseTexture9* texture = nullptr;
    FogMaterialResources resources;
    DWORD samplerStates[kAtlasSamplerStateCount] = {};
    DWORD fogEnabled = FALSE;
    float constants[kFogMaterialConstantCount][4] = {};
    bool changed = false;

    void Clear()
    {
        ReleasePointer(vertex);
        ReleasePointer(pixel);
        ReleasePointer(texture);
        device = nullptr;
        changed = false;
    }

    ~SavedDraw() { Clear(); }
};

MaterialFog::MaterialFog() : m_saved(std::make_unique<SavedDraw>()) {}

MaterialFog::~MaterialFog()
{
    Release();
}

bool MaterialFog::Begin(const MaterialFogVolume& volume)
{
    End();
    if (!m_compatible || !volume.atlas || !volume.target || !volume.depth || !volume.width || !volume.height ||
        volume.slices < 2 || !volume.columns || !volume.rows ||
        volume.columns * static_cast<unsigned long long>(volume.rows) < volume.slices ||
        !std::isfinite(volume.maxDistance) || volume.maxDistance <= 0 ||
        !std::isfinite(volume.exposure) || !std::isfinite(volume.glow))
        return false;
    m_volume = volume;
    m_appliedDraws = 0;
    m_unsupportedDraws = 0;
    return true;
}

void MaterialFog::End()
{
    Restore();
    if (m_volume.atlas && m_unsupportedDraws)
        VF_LOG_INFO("material fog compatibility fallback: %u fogged draws, %u unsupported draws; "
                    "using post-world fog from the next frame until the device resets",
                    m_appliedDraws, m_unsupportedDraws);
    m_volume = {};
}

void MaterialFog::Release()
{
    End();
    m_pairs.clear();
    m_compatible = true;
    m_fallbacksLogged = 0;
}

void MaterialFog::LogFallback(const char* reason)
{
    m_compatible = false;
    ++m_unsupportedDraws;
    if (m_fallbacksLogged++ < kMaterialFallbackLogLimit)
        VF_LOG_INFO("material fog left a draw unchanged: %s", reason);
}

bool MaterialFog::MatchesWorldDraw(IDirect3DDevice9* device) const
{
    if (!m_volume.atlas)
        return false;
    D3DVIEWPORT9 viewport = {};
    DWORD depthEnabled = FALSE;
    DWORD colourMask = 0;
    if (FAILED(device->GetViewport(&viewport)) || std::memcmp(&viewport, &m_volume.viewport, sizeof(viewport)) != 0 ||
        FAILED(device->GetRenderState(D3DRS_ZENABLE, &depthEnabled)) || depthEnabled != D3DZB_TRUE ||
        FAILED(device->GetRenderState(D3DRS_COLORWRITEENABLE, &colourMask)) || !(colourMask & 7))
        return false;
    IDirect3DSurface9* target = nullptr;
    IDirect3DSurface9* depth = nullptr;
    const bool matches = SUCCEEDED(device->GetRenderTarget(0, &target)) && target == m_volume.target &&
                         SUCCEEDED(device->GetDepthStencilSurface(&depth)) && depth == m_volume.depth;
    ReleasePointer(target);
    ReleasePointer(depth);
    return matches;
}

bool MaterialFog::ReadBlendMode(IDirect3DDevice9* device, float& premultiplied, float& additive)
{
    premultiplied = 0;
    additive = 0;
    DWORD enabled = FALSE;
    if (FAILED(device->GetRenderState(D3DRS_ALPHABLENDENABLE, &enabled)))
        return false;
    if (!enabled)
        return true;
    DWORD source = 0;
    DWORD destination = 0;
    DWORD operation = 0;
    if (FAILED(device->GetRenderState(D3DRS_SRCBLEND, &source)) ||
        FAILED(device->GetRenderState(D3DRS_DESTBLEND, &destination)) ||
        FAILED(device->GetRenderState(D3DRS_BLENDOP, &operation)))
        return false;
    if (operation == D3DBLENDOP_ADD)
    {
        if (source == D3DBLEND_SRCALPHA && destination == D3DBLEND_INVSRCALPHA)
            return true;
        if (source == D3DBLEND_ONE && destination == D3DBLEND_INVSRCALPHA)
        {
            premultiplied = 1;
            return true;
        }
        if ((source == D3DBLEND_ONE || source == D3DBLEND_SRCALPHA) && destination == D3DBLEND_ONE)
        {
            additive = 1;
            return true;
        }
        if (source == D3DBLEND_ONE && destination == D3DBLEND_ZERO)
            return true;
    }
    LogFallback("unsupported colour blend equation");
    return false;
}

MaterialFog::ShaderPair* MaterialFog::FindOrCreatePair(IDirect3DDevice9* device, IDirect3DVertexShader9* vertex,
                                                      IDirect3DPixelShader9* pixel)
{
    ++m_serial;
    for (const auto& pair : m_pairs)
        if (pair->originalVertex == vertex && pair->originalPixel == pixel)
        {
            pair->used = m_serial;
            if (!pair->ready)
                ++m_unsupportedDraws;
            return pair.get();
        }
    if (m_pairs.size() >= kMaterialPairCacheLimit)
        m_pairs.erase(std::min_element(m_pairs.begin(), m_pairs.end(), [](const auto& left, const auto& right) {
            return left->used < right->used;
        }));
    auto pair = std::make_unique<ShaderPair>();
    pair->originalVertex = vertex;
    pair->originalPixel = pixel;
    vertex->AddRef();
    pixel->AddRef();
    pair->used = m_serial;
    std::vector<DWORD> vertexCode;
    std::vector<DWORD> pixelCode;
    FogMaterialShaders shaders;
    std::string failure;
    if (!ReadShader(vertex, vertexCode) || !ReadShader(pixel, pixelCode))
        failure = "material bytecode could not be read";
    else if (InstrumentFogMaterialShaders(vertexCode.data(), vertexCode.size(), pixelCode.data(), pixelCode.size(),
                                          shaders, failure))
    {
        if (SUCCEEDED(device->CreateVertexShader(shaders.vertex.data(), &pair->fogVertex)) &&
            SUCCEEDED(device->CreatePixelShader(shaders.pixel.data(), &pair->fogPixel)))
        {
            pair->ready = true;
            pair->resources = shaders.resources;
        }
        else
            failure = "the driver rejected the instrumented material shaders";
    }
    if (!pair->ready)
        LogFallback(failure.c_str());
    m_pairs.push_back(std::move(pair));
    return m_pairs.back().get();
}

bool MaterialFog::Apply(IDirect3DDevice9* device)
{
    if (!device || !MatchesWorldDraw(device))
        return false;
    IDirect3DSurface9* additionalTarget = nullptr;
    if (SUCCEEDED(device->GetRenderTarget(1, &additionalTarget)) && additionalTarget)
    {
        additionalTarget->Release();
        LogFallback("multiple render targets are active");
        return false;
    }
    float premultiplied = 0;
    float additive = 0;
    if (!ReadBlendMode(device, premultiplied, additive))
        return false;
    Restore();
    SavedDraw& saved = *m_saved;
    if (FAILED(device->GetVertexShader(&saved.vertex)) || FAILED(device->GetPixelShader(&saved.pixel)) ||
        !saved.vertex || !saved.pixel)
    {
        saved.Clear();
        LogFallback("fixed function materials have no programmable shader pair");
        return false;
    }
    ShaderPair* pair = nullptr;
    try
    {
        pair = FindOrCreatePair(device, saved.vertex, saved.pixel);
    }
    catch (const std::bad_alloc&)
    {
        saved.Clear();
        LogFallback("material shader allocation failed");
        return false;
    }
    if (!pair->ready)
    {
        saved.Clear();
        return false;
    }
    saved.resources = pair->resources;
    bool read = SUCCEEDED(device->GetPixelShaderConstantF(saved.resources.constantBase, &saved.constants[0][0],
                                                          kFogMaterialConstantCount)) &&
                SUCCEEDED(device->GetTexture(saved.resources.sampler, &saved.texture)) &&
                SUCCEEDED(device->GetRenderState(D3DRS_FOGENABLE, &saved.fogEnabled));
    for (size_t index = 0; index < kAtlasSamplerStateCount && read; ++index)
        read = SUCCEEDED(device->GetSamplerState(saved.resources.sampler, kAtlasSamplerStates[index],
                                                  &saved.samplerStates[index]));
    if (!read)
    {
        saved.Clear();
        LogFallback("material state could not be saved");
        return false;
    }
    saved.device = device;
    saved.changed = true;
    const float uniforms[kFogMaterialConstantCount][4] = {
        {1.0f / m_volume.maxDistance, static_cast<float>(m_volume.slices - 1),
         static_cast<float>(m_volume.columns), 1.0f / m_volume.columns},
        {1.0f / m_volume.rows, 0.5f / m_volume.width, 0.5f / m_volume.height, m_volume.exposure},
        {premultiplied, additive, m_volume.linear ? (m_volume.sceneCopy ? 1.0f : 2.0f) : 0.0f, m_volume.glow},
    };
    bool applied = SUCCEEDED(device->SetVertexShader(pair->fogVertex)) &&
                   SUCCEEDED(device->SetPixelShader(pair->fogPixel)) &&
                   SUCCEEDED(device->SetTexture(saved.resources.sampler, m_volume.atlas)) &&
                   SUCCEEDED(device->SetPixelShaderConstantF(saved.resources.constantBase, &uniforms[0][0],
                                                             kFogMaterialConstantCount)) &&
                   SUCCEEDED(device->SetRenderState(D3DRS_FOGENABLE, FALSE));
    for (size_t index = 0; index < kAtlasSamplerStateCount && applied; ++index)
        applied = SUCCEEDED(device->SetSamplerState(saved.resources.sampler, kAtlasSamplerStates[index],
                                                     kAtlasSamplerValues[index]));
    if (!applied)
    {
        Restore();
        LogFallback("material fog state could not be bound");
    }
    else
        ++m_appliedDraws;
    return applied;
}

void MaterialFog::Restore()
{
    SavedDraw& saved = *m_saved;
    if (saved.changed)
    {
        IDirect3DDevice9* device = saved.device;
        device->SetVertexShader(saved.vertex);
        device->SetPixelShader(saved.pixel);
        device->SetPixelShaderConstantF(saved.resources.constantBase, &saved.constants[0][0],
                                        kFogMaterialConstantCount);
        device->SetTexture(saved.resources.sampler, saved.texture);
        for (size_t index = 0; index < kAtlasSamplerStateCount; ++index)
            device->SetSamplerState(saved.resources.sampler, kAtlasSamplerStates[index], saved.samplerStates[index]);
        device->SetRenderState(D3DRS_FOGENABLE, saved.fogEnabled);
    }
    saved.Clear();
}
