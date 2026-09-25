#include "material_fog.h"

#include "fixed_function_material.h"
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

void LogLegacyMaterialState(IDirect3DDevice9* device, bool vertex, bool pixel)
{
    DWORD fvf = 0;
    DWORD lighting = 0;
    DWORD blend = 0;
    DWORD source = 0;
    DWORD destination = 0;
    device->GetFVF(&fvf);
    device->GetRenderState(D3DRS_LIGHTING, &lighting);
    device->GetRenderState(D3DRS_ALPHABLENDENABLE, &blend);
    device->GetRenderState(D3DRS_SRCBLEND, &source);
    device->GetRenderState(D3DRS_DESTBLEND, &destination);
    VF_LOG_INFO("legacy material state: missing VS %u, PS %u; FVF %08lX, lighting %lu, blend %lu/%lu/%lu",
                vertex ? 1u : 0u, pixel ? 1u : 0u, fvf, lighting, blend, source, destination);
    IDirect3DVertexDeclaration9* declaration = nullptr;
    if (SUCCEEDED(device->GetVertexDeclaration(&declaration)) && declaration)
    {
        D3DVERTEXELEMENT9 elements[MAXD3DDECLLENGTH + 1] = {};
        UINT count = MAXD3DDECLLENGTH + 1;
        if (SUCCEEDED(declaration->GetDeclaration(elements, &count)))
            for (UINT index = 0; index < count && elements[index].Stream != 0xFF; ++index)
            {
                const auto& element = elements[index];
                VF_LOG_INFO("legacy material input: stream %u offset %u type %u usage %u index %u",
                            element.Stream, element.Offset, element.Type, element.Usage, element.UsageIndex);
            }
        declaration->Release();
    }
    for (DWORD stage = 0; stage < kFixedFunctionTextureStages; ++stage)
    {
        DWORD colour = D3DTOP_DISABLE;
        DWORD alpha = D3DTOP_DISABLE;
        DWORD coordinate = 0;
        DWORD transform = 0;
        device->GetTextureStageState(stage, D3DTSS_COLOROP, &colour);
        device->GetTextureStageState(stage, D3DTSS_ALPHAOP, &alpha);
        device->GetTextureStageState(stage, D3DTSS_TEXCOORDINDEX, &coordinate);
        device->GetTextureStageState(stage, D3DTSS_TEXTURETRANSFORMFLAGS, &transform);
        if (colour == D3DTOP_DISABLE)
            break;
        VF_LOG_INFO("legacy material stage %lu: colour %lu, alpha %lu, coordinates %08lX, transform %lu",
                    stage, colour, alpha, coordinate, transform);
    }
}
}

struct MaterialFog::ShaderPair
{
    IDirect3DVertexShader9* originalVertex = nullptr;
    IDirect3DPixelShader9* originalPixel = nullptr;
    IDirect3DVertexShader9* fogVertex = nullptr;
    IDirect3DPixelShader9* fogPixel = nullptr;
    FogMaterialResources resources;
    std::vector<DWORD> fixedKey;
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
    float vertexConstants[kFixedFunctionVertexConstantCount][4] = {};
    float pixelConstants[kFixedFunctionPixelConstantCount][4] = {};
    UINT vertexConstantCount = 0;
    UINT pixelConstantCount = 0;
    bool changed = false;

    void Clear()
    {
        ReleasePointer(vertex);
        ReleasePointer(pixel);
        ReleasePointer(texture);
        device = nullptr;
        vertexConstantCount = 0;
        pixelConstantCount = 0;
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
                    "using post-world fog until Material fog is toggled off/on or the device resets",
                    m_appliedDraws, m_unsupportedDraws);
    m_volume = {};
}

void MaterialFog::Release()
{
    End();
    m_pairs.clear();
    m_compatible = true;
    m_fallbacksLogged = 0;
    m_failure.clear();
}

void MaterialFog::SetRequested(bool requested)
{
    if (requested && !m_requested && !m_compatible)
    {
        Release();
        VF_LOG_INFO("material fog compatibility retry requested");
    }
    m_requested = requested;
}

void MaterialFog::LogFallback(const char* reason)
{
    m_compatible = false;
    m_failure = reason;
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
                                                      IDirect3DPixelShader9* pixel,
                                                      const FixedFunctionMaterialState& fixed)
{
    ++m_serial;
    for (const auto& pair : m_pairs)
        if (pair->originalVertex == vertex && pair->originalPixel == pixel && pair->fixedKey == fixed.key)
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
    if (vertex)
        vertex->AddRef();
    if (pixel)
        pixel->AddRef();
    pair->fixedKey = fixed.key;
    pair->used = m_serial;
    std::vector<DWORD> vertexCode;
    std::vector<DWORD> pixelCode;
    FogMaterialShaders shaders;
    std::string failure;
    const bool generated = (!fixed.vertex && !fixed.pixel) ||
                           BuildFixedFunctionMaterialShaders(fixed, vertexCode, pixelCode, failure);
    if (generated && ((vertex && !ReadShader(vertex, vertexCode)) || (pixel && !ReadShader(pixel, pixelCode))))
        failure = "material bytecode could not be read";
    else if (generated && InstrumentFogMaterialShaders(vertexCode.data(), vertexCode.size(),
                                          pixelCode.data(), pixelCode.size(),
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
    {
        if ((fixed.vertex || fixed.pixel) && m_fallbacksLogged < kMaterialFallbackLogLimit)
            LogLegacyMaterialState(device, fixed.vertex, fixed.pixel);
        LogFallback(failure.c_str());
    }
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
    if (FAILED(device->GetVertexShader(&saved.vertex)) || FAILED(device->GetPixelShader(&saved.pixel)))
    {
        saved.Clear();
        LogFallback("material shader bindings could not be read");
        return false;
    }
    ShaderPair* pair = nullptr;
    FixedFunctionMaterialState fixed;
    try
    {
        std::string failure;
        if ((!saved.vertex || !saved.pixel) &&
            !CaptureFixedFunctionMaterial(device, !saved.vertex, !saved.pixel, fixed, failure))
        {
            if (m_fallbacksLogged < kMaterialFallbackLogLimit)
                LogLegacyMaterialState(device, !saved.vertex, !saved.pixel);
            saved.Clear();
            LogFallback(failure.c_str());
            return false;
        }
        pair = FindOrCreatePair(device, saved.vertex, saved.pixel, fixed);
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
    saved.vertexConstantCount = fixed.vertexConstantCount;
    saved.pixelConstantCount = fixed.pixelConstantCount;
    bool read = SUCCEEDED(device->GetPixelShaderConstantF(saved.resources.constantBase, &saved.constants[0][0],
                                                          kFogMaterialConstantCount)) &&
                SUCCEEDED(device->GetTexture(saved.resources.sampler, &saved.texture)) &&
                SUCCEEDED(device->GetRenderState(D3DRS_FOGENABLE, &saved.fogEnabled));
    for (size_t index = 0; index < kAtlasSamplerStateCount && read; ++index)
        read = SUCCEEDED(device->GetSamplerState(saved.resources.sampler, kAtlasSamplerStates[index],
                                                  &saved.samplerStates[index]));
    if (read && saved.vertexConstantCount)
        read = SUCCEEDED(device->GetVertexShaderConstantF(0, &saved.vertexConstants[0][0],
                                                           saved.vertexConstantCount));
    if (read && saved.pixelConstantCount)
        read = SUCCEEDED(device->GetPixelShaderConstantF(0, &saved.pixelConstants[0][0],
                                                          saved.pixelConstantCount));
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
                   SUCCEEDED(device->SetPixelShader(pair->fogPixel));
    if (applied && fixed.vertexConstantCount)
        applied = SUCCEEDED(device->SetVertexShaderConstantF(0, &fixed.vertexConstants[0][0],
                                                              fixed.vertexConstantCount));
    if (applied && fixed.pixelConstantCount)
        applied = SUCCEEDED(device->SetPixelShaderConstantF(0, &fixed.pixelConstants[0][0],
                                                             fixed.pixelConstantCount));
    applied = applied &&
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
        if (saved.vertexConstantCount)
            device->SetVertexShaderConstantF(0, &saved.vertexConstants[0][0], saved.vertexConstantCount);
        if (saved.pixelConstantCount)
            device->SetPixelShaderConstantF(0, &saved.pixelConstants[0][0], saved.pixelConstantCount);
        device->SetTexture(saved.resources.sampler, saved.texture);
        for (size_t index = 0; index < kAtlasSamplerStateCount; ++index)
            device->SetSamplerState(saved.resources.sampler, kAtlasSamplerStates[index], saved.samplerStates[index]);
        device->SetRenderState(D3DRS_FOGENABLE, saved.fogEnabled);
    }
    saved.Clear();
}
