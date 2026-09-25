#pragma once

#include "shader_instrumentation.h"

#include <initializer_list>

namespace shader_instrumentation_checks
{
DWORD Reg(unsigned type, unsigned index, DWORD components)
{
    return 0x80000000u | ((type << D3DSP_REGTYPE_SHIFT) & D3DSP_REGTYPE_MASK) |
           ((type << D3DSP_REGTYPE_SHIFT2) & D3DSP_REGTYPE_MASK2) | index | components;
}

DWORD Dst(unsigned type, unsigned index, DWORD mask = D3DSP_WRITEMASK_ALL) { return Reg(type, index, mask); }

DWORD Src(unsigned type, unsigned index, DWORD swizzle = D3DSP_NOSWIZZLE) { return Reg(type, index, swizzle); }

void Emit(std::vector<DWORD>& code, DWORD opcode, std::initializer_list<DWORD> parameters)
{
    code.push_back(opcode | (static_cast<DWORD>(parameters.size()) << D3DSI_INSTLENGTH_SHIFT));
    code.insert(code.end(), parameters.begin(), parameters.end());
}

void Declare(std::vector<DWORD>& code, unsigned type, unsigned reg, unsigned usage, unsigned semantic = 0,
              DWORD mask = D3DSP_WRITEMASK_ALL)
{
    Emit(code, D3DSIO_DCL, {0x80000000u | usage | (semantic << D3DSP_DCL_USAGEINDEX_SHIFT), Dst(type, reg, mask)});
}

std::vector<DWORD> Vertex(unsigned major, bool relative = false, bool occupied = false)
{
    std::vector<DWORD> code = {D3DVS_VERSION(major, 0)};
    Declare(code, D3DSPR_INPUT, 0, D3DDECLUSAGE_POSITION);
    const unsigned output = major == 3 ? D3DSPR_OUTPUT : D3DSPR_RASTOUT;
    if (major == 3)
        Declare(code, output, 0, D3DDECLUSAGE_POSITION);
    if (occupied && major == 3)
        Declare(code, D3DSPR_OUTPUT, 1, D3DDECLUSAGE_TEXCOORD, kFogMaterialClipTexcoord);
    if (relative)
    {
        Emit(code, D3DSIO_MOVA, {Dst(D3DSPR_ADDR, 0, D3DSP_WRITEMASK_0), Src(D3DSPR_CONST, 19, 0)});
        Emit(code, D3DSIO_MUL,
              {Dst(D3DSPR_TEMP, 0), Src(D3DSPR_INPUT, 0),
               Src(D3DSPR_CONST, 20) | D3DSHADER_ADDRMODE_RELATIVE, Src(D3DSPR_ADDR, 0, 0)});
    }
    else
        Emit(code, D3DSIO_MOV, {Dst(D3DSPR_TEMP, 0), Src(D3DSPR_INPUT, 0)});
    Emit(code, D3DSIO_MOV, {Dst(output, 0, D3DSP_WRITEMASK_0 | D3DSP_WRITEMASK_1), Src(D3DSPR_TEMP, 0)});
    Emit(code, D3DSIO_MOV, {Dst(output, 0, D3DSP_WRITEMASK_2 | D3DSP_WRITEMASK_3), Src(D3DSPR_TEMP, 0)});
    if (occupied)
        Emit(code, D3DSIO_MOV,
              {Dst(D3DSPR_OUTPUT, major == 3 ? 1 : kFogMaterialClipTexcoord), Src(D3DSPR_TEMP, 0)});
    code.push_back(D3DSIO_END);
    return code;
}

std::vector<DWORD> Pixel(unsigned major, bool branch = false, bool occupied = false, bool collision = false)
{
    std::vector<DWORD> code = {D3DPS_VERSION(major, 0)};
    if (occupied)
        Declare(code, major == 3 ? D3DSPR_INPUT : D3DSPR_TEXTURE, major == 3 ? 0 : kFogMaterialClipTexcoord,
                 D3DDECLUSAGE_TEXCOORD, kFogMaterialClipTexcoord);
    if (collision)
    {
        Emit(code, D3DSIO_DEF, {Dst(D3DSPR_CONST, 223), 0x3F800000u, 0, 0, 0});
        Emit(code, D3DSIO_DCL, {0x90000000u, Dst(D3DSPR_SAMPLER, 15)});
    }
    const unsigned temporary = collision ? 31u : 0u;
    Emit(code, D3DSIO_MOV, {Dst(D3DSPR_TEMP, temporary), Src(D3DSPR_CONST, 0)});
    if (branch)
        Emit(code, D3DSIO_IFC | (D3DSPC_GT << 16), {Src(D3DSPR_CONST, 1, 0), Src(D3DSPR_CONST, 1, 0x00550000u)});
    Emit(code, D3DSIO_MOV,
          {Dst(D3DSPR_COLOROUT, 0, D3DSP_WRITEMASK_0 | D3DSP_WRITEMASK_1 | D3DSP_WRITEMASK_2),
           Src(D3DSPR_TEMP, temporary)});
    if (branch)
    {
        Emit(code, D3DSIO_ELSE, {});
        Emit(code, D3DSIO_MUL,
              {Dst(D3DSPR_COLOROUT, 0, D3DSP_WRITEMASK_0 | D3DSP_WRITEMASK_1 | D3DSP_WRITEMASK_2),
               Src(D3DSPR_TEMP, temporary), Src(D3DSPR_CONST, 2)});
        Emit(code, D3DSIO_ENDIF, {});
    }
    Emit(code, D3DSIO_MOV, {Dst(D3DSPR_COLOROUT, 0, D3DSP_WRITEMASK_3), Src(D3DSPR_TEMP, temporary)});
    code.push_back(D3DSIO_END);
    return code;
}

std::vector<DWORD> WideVertex(unsigned major)
{
    auto code = Vertex(major);
    std::vector<DWORD> declarations;
    std::vector<DWORD> writes;
    for (unsigned index = 0; index < 10; ++index)
    {
        const unsigned usage = index < 2 ? D3DDECLUSAGE_COLOR : index < 9 ? D3DDECLUSAGE_TEXCOORD
                                                                                       : D3DDECLUSAGE_FOG;
        const unsigned semantic = index < 2 ? index : index < 9 ? index - 2 : 0;
        const unsigned type = major == 3 ? D3DSPR_OUTPUT : index < 2 ? D3DSPR_ATTROUT
                                                                     : index < 9 ? D3DSPR_TEXCRDOUT : D3DSPR_RASTOUT;
        const unsigned reg = major == 3 ? index + 1 : index < 2 ? index : index < 9 ? index - 2 : 1;
        const DWORD mask = index == 9 ? D3DSP_WRITEMASK_0 : D3DSP_WRITEMASK_ALL;
        if (major == 3)
            Declare(declarations, type, reg, usage, semantic, mask);
        Emit(writes, D3DSIO_MOV, {Dst(type, reg, mask), Src(D3DSPR_CONST, 20)});
    }
    code.insert(code.begin() + 1, declarations.begin(), declarations.end());
    code.insert(code.end() - 1, writes.begin(), writes.end());
    return code;
}

std::vector<DWORD> WidePixel(unsigned major)
{
    auto code = Pixel(major);
    std::vector<DWORD> declarations;
    for (unsigned index = 0; index < (major == 3 ? 10u : 9u); ++index)
    {
        const unsigned usage = index < 2 ? D3DDECLUSAGE_COLOR : index < 9 ? D3DDECLUSAGE_TEXCOORD
                                                                                       : D3DDECLUSAGE_FOG;
        const unsigned semantic = index < 2 ? index : index < 9 ? index - 2 : 0;
        const unsigned type = major == 3 || index < 2 ? D3DSPR_INPUT : D3DSPR_TEXTURE;
        const unsigned reg = major == 3 || index < 2 ? index : index - 2;
        Declare(declarations, type, reg, usage, semantic,
                 index == 9 ? D3DSP_WRITEMASK_0 : D3DSP_WRITEMASK_ALL);
    }
    code.insert(code.begin() + 1, declarations.begin(), declarations.end());
    return code;
}

struct Fixture
{
    IDirect3DDevice9* dev;
    IDirect3DStateBlock9* state = nullptr;
    IDirect3DSurface9* previousTarget = nullptr;
    IDirect3DSurface9* previousDepth = nullptr;
    IDirect3DVertexBuffer9* previousStream = nullptr;
    UINT previousOffset = 0;
    UINT previousStride = 0;
    IDirect3DTexture9* atlas = nullptr;
    IDirect3DSurface9* target = nullptr;
    IDirect3DSurface9* readback = nullptr;
    IDirect3DVertexDeclaration9* declaration = nullptr;

    explicit Fixture(IDirect3DDevice9* device) : dev(device) {}

    ~Fixture()
    {
        if (previousTarget)
            dev->SetRenderTarget(0, previousTarget);
        dev->SetDepthStencilSurface(previousDepth);
        if (state)
            state->Apply();
        dev->SetStreamSource(0, previousStream, previousOffset, previousStride);
        IUnknown* resources[] = {state, previousTarget, previousDepth, previousStream, atlas, target, readback,
                                 declaration};
        for (IUnknown* resource : resources)
            if (resource)
                resource->Release();
    }

    bool Create()
    {
        if (FAILED(dev->CreateStateBlock(D3DSBT_ALL, &state)))
            return false;
        dev->GetRenderTarget(0, &previousTarget);
        dev->GetDepthStencilSurface(&previousDepth);
        dev->GetStreamSource(0, &previousStream, &previousOffset, &previousStride);
        const D3DVERTEXELEMENT9 elements[] = {
            {0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0}, D3DDECL_END()};
        if (FAILED(dev->CreateTexture(8, 8, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &atlas, nullptr)) ||
            FAILED(dev->CreateRenderTarget(8, 8, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE,
                                          &target, nullptr)) ||
            FAILED(dev->CreateOffscreenPlainSurface(8, 8, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM,
                                                    &readback, nullptr)) ||
            FAILED(dev->CreateVertexDeclaration(elements, &declaration)))
            return false;
        D3DLOCKED_RECT locked = {};
        if (FAILED(atlas->LockRect(0, &locked, nullptr, 0)))
            return false;
        for (unsigned y = 0; y < 8; ++y)
        {
            auto* row = reinterpret_cast<DWORD*>(static_cast<BYTE*>(locked.pBits) + y * locked.Pitch);
            for (unsigned x = 0; x < 8; ++x)
            {
                const unsigned slice = x / 4 + (y / 4) * 2;
                row[x] = (slice * 64u << 24) | (slice * 32u << 16) | (slice * 16u << 8) | (slice * 8u);
            }
        }
        return SUCCEEDED(atlas->UnlockRect(0));
    }

    bool Render(const FogMaterialShaders& shaders, float viewDepth, const float* source, int mode, bool linear,
                 bool alternateBranch, float* rgba, float glow = 0.0f, float exposure = 1.0f, int colourMode = -1)
    {
        IDirect3DVertexShader9* vs = nullptr;
        IDirect3DPixelShader9* ps = nullptr;
        if (FAILED(dev->CreateVertexShader(shaders.vertex.data(), &vs)) ||
            FAILED(dev->CreatePixelShader(shaders.pixel.data(), &ps)))
        {
            if (vs)
                vs->Release();
            return false;
        }
        const D3DVIEWPORT9 viewport = {0, 0, 8, 8, 0, 1};
        dev->SetDepthStencilSurface(nullptr);
        dev->SetRenderTarget(0, target);
        dev->SetViewport(&viewport);
        dev->SetVertexDeclaration(declaration);
        dev->SetVertexShader(vs);
        dev->SetPixelShader(ps);
        dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
        dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
        dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        dev->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
        dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
        const float material[3][4] = {{1.0f / 9.0f, 3, 2, 0.5f}, {0.5f, 0.0625f, 0.0625f, exposure},
                                      {mode == 1 ? 1.0f : 0.0f, mode == 2 ? 1.0f : 0.0f,
                                       colourMode >= 0 ? static_cast<float>(colourMode) : linear ? 1.0f : 0.0f, glow}};
        const float branch[4] = {alternateBranch ? -1.0f : 1.0f, 0, 0, 0};
        const float branchMultiplier[4] = {0.5f, 0.5f, 0.5f, 1};
        const float vertexConstants[2][4] = {{0, 0, 0, 0}, {1, 1, 1, 1}};
        dev->SetVertexShaderConstantF(19, &vertexConstants[0][0], 2);
        dev->SetPixelShaderConstantF(0, source, 1);
        dev->SetPixelShaderConstantF(1, branch, 1);
        dev->SetPixelShaderConstantF(2, branchMultiplier, 1);
        dev->SetPixelShaderConstantF(shaders.resources.constantBase, &material[0][0], kFogMaterialConstantCount);
        dev->SetTexture(shaders.resources.sampler, atlas);
        dev->SetSamplerState(shaders.resources.sampler, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        dev->SetSamplerState(shaders.resources.sampler, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        dev->SetSamplerState(shaders.resources.sampler, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        dev->SetSamplerState(shaders.resources.sampler, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        dev->SetSamplerState(shaders.resources.sampler, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        dev->SetSamplerState(shaders.resources.sampler, D3DSAMP_SRGBTEXTURE, FALSE);
        const float vertices[4][4] = {{-viewDepth, viewDepth, viewDepth * 0.5f, viewDepth},
                                      {viewDepth, viewDepth, viewDepth * 0.5f, viewDepth},
                                      {-viewDepth, -viewDepth, viewDepth * 0.5f, viewDepth},
                                      {viewDepth, -viewDepth, viewDepth * 0.5f, viewDepth}};
        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, 0xFF000000u, 1, 0);
        const HRESULT draw = dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(vertices[0]));
        dev->EndScene();
        dev->SetTexture(shaders.resources.sampler, nullptr);
        vs->Release();
        ps->Release();
        if (FAILED(draw) || FAILED(dev->GetRenderTargetData(target, readback)))
            return false;
        D3DLOCKED_RECT locked = {};
        if (FAILED(readback->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
            return false;
        const auto* pixel = static_cast<const BYTE*>(locked.pBits) + 4 * locked.Pitch + 4 * 4;
        for (int channel = 0; channel < 4; ++channel)
            rgba[channel] = pixel[channel < 3 ? 2 - channel : channel] / 255.0f;
        readback->UnlockRect();
        return true;
    }
};

void Run(IDirect3DDevice9* device)
{
    Fixture fixture(device);
    Check(fixture.Create(), "transparent material shader fixture created");
    if (!fixture.atlas || !fixture.target || !fixture.readback || !fixture.declaration)
        return;
    for (unsigned major : {2u, 3u})
    {
        const auto vertex = Vertex(major, true);
        const auto pixel = Pixel(major, major == 3, false, major == 3);
        FogMaterialShaders instrumented;
        std::string reason;
        const bool rewritten = InstrumentFogMaterialShaders(vertex.data(), vertex.size(), pixel.data(), pixel.size(),
                                                             instrumented, reason);
        Check(rewritten, major == 2 ? "shader2 material pair upgrades with relative bone constants"
                                   : "shader3 material pair preserves partial output writes and branches");
        if (!rewritten)
        {
            std::printf("     material rewrite: %s\n", reason.c_str());
            continue;
        }
        Check(major == 2 || instrumented.resources.sampler != 15,
              "transparent fog allocates resources around occupied material registers");
        for (int mode = 0; mode < 3; ++mode)
        {
            float largestError = 0;
            bool rendered = true;
            for (float depth : {1.0f, 2.25f, 4.0f})
                for (bool linear : {false, true})
                {
                    const float source[4] = {0.4f, 0.2f, 0.1f, 0.4f};
                    float actual[4] = {};
                    const bool alternate = major == 3 && depth == 2.25f;
                    rendered = fixture.Render(instrumented, depth, source, mode, linear, alternate, actual) && rendered;
                    const float slice = std::sqrt(depth);
                    const float transmittance = 1.0f - slice * 64.0f / 255.0f;
                    const float association = mode == 1 ? source[3] : 1;
                    const float gamma = linear ? 2.2f : 1.0f;
                    for (int channel = 0; channel < 3; ++channel)
                    {
                        const float colour = source[channel] * (alternate ? 0.5f : 1.0f) / association;
                        const float radiance = mode == 2 ? 0 : slice * static_cast<float>(32u >> channel) / 255.0f;
                        const float expected = std::pow(std::pow(colour, gamma) * transmittance + radiance,
                                                        1.0f / gamma) * association;
                        largestError = std::fmax(largestError, std::fabs(actual[channel] - expected));
                    }
                    largestError = std::fmax(largestError, std::fabs(actual[3] - source[3]));
                }
            char label[160];
            std::snprintf(label, sizeof(label), "shader%u %s material fog uses fragment depth and preserves alpha",
                          major, mode == 0 ? "alpha" : mode == 1 ? "premultiplied" : "additive");
            Check(rendered && largestError <= 3.0f / 255.0f, label);
            if (!rendered || largestError > 3.0f / 255.0f)
                std::printf("     material render %d, maximum error %.3f/255\n", rendered, largestError * 255);
        }
        FogMaterialShaders rejected;
        const auto occupiedVertex = Vertex(major, false, true);
        Check(!InstrumentFogMaterialShaders(occupiedVertex.data(), occupiedVertex.size(), pixel.data(), pixel.size(),
                                             rejected, reason) && rejected.vertex.empty() && rejected.pixel.empty(),
              "occupied material TEXCOORD7 is rejected without partial replacement");
        auto truncated = pixel;
        truncated.pop_back();
        Check(!InstrumentFogMaterialShaders(vertex.data(), vertex.size(), truncated.data(), truncated.size(),
                                             rejected, reason),
              "truncated material shader is rejected");
        auto subroutine = pixel;
        subroutine.insert(subroutine.end() - 1, D3DSIO_RET);
        Check(!InstrumentFogMaterialShaders(vertex.data(), vertex.size(), subroutine.data(), subroutine.size(),
                                             rejected, reason),
              "unsafe material subroutine flow is rejected");
        const auto wideVertex = WideVertex(major);
        const auto widePixel = WidePixel(major);
        const bool packed = InstrumentFogMaterialShaders(wideVertex.data(), wideVertex.size(), widePixel.data(),
                                                          widePixel.size(), instrumented, reason);
        const float source[4] = {0.4f, 0.2f, 0.1f, 0.4f};
        float actual[4] = {};
        const bool drawn = packed && fixture.Render(instrumented, 4, source, 0, false, false, actual);
        const float expected = source[0] * (1 - 128.0f / 255.0f) + 64.0f / 255.0f;
        Check(drawn && std::fabs(actual[0] - expected) <= 2.0f / 255.0f,
              "full interpolation budget packs fragment depth beside the existing fog factor");
        const float highlight[4] = {0.9f, 0.8f, 0.7f, 1};
        const bool highlighted = packed && fixture.Render(instrumented, 9, highlight, 0, true, false, actual, 0.6f, 4);
        float largestHighlightError = 0;
        for (int channel = 0; channel < 3; ++channel)
        {
            const float scene = std::pow(highlight[channel], 2.2f);
            const float linear = scene * (1 - 192.0f / 255.0f) + (96.0f / 255.0f) * (4.0f / (1u << channel));
            const float knee = std::fmax(0.8f, scene);
            const float span = std::fmax(1 - knee, 1e-4f);
            const float rolled = linear <= knee ? linear : knee + span * (1 - std::exp(-(linear - knee) / span));
            const float display = std::pow(rolled, 1.0f / 2.2f);
            const float beforeGlow = 2 * display / (std::sqrt(1 + 2.4f * display) + 1);
            const float expectedHighlight = display + (beforeGlow - display) * (192.0f / 255.0f);
            largestHighlightError = std::fmax(largestHighlightError, std::fabs(actual[channel] - expectedHighlight));
        }
        Check(highlighted && largestHighlightError < 3.0f / 255.0f,
              "material highlights and glow compensation match the opaque composite");
        const float transparent[4] = {0, 0, 0, 0};
        const bool invisible = packed && fixture.Render(instrumented, 4, transparent, 1, true, false, actual);
        Check(invisible && actual[0] == 0 && actual[1] == 0 && actual[2] == 0 && actual[3] == 0,
              "zero-alpha premultiplied materials add no fog radiance");
        for (int colourMode : {0, 2})
        {
            const bool fallback = packed && fixture.Render(instrumented, 4, highlight, 0, true, false, actual,
                                                             0, 4, colourMode);
            float largestError = 0;
            for (unsigned channel = 0; channel < 3; ++channel)
            {
                const float unassociatedFog = 2.0f / (1u << channel);
                float rolled = unassociatedFog <= 0.8f ? unassociatedFog :
                               0.8f + 0.2f * (1 - std::exp(-(unassociatedFog - 0.8f) / 0.2f));
                if (colourMode == 2)
                    rolled = std::pow(rolled, 1.0f / 2.2f);
                const float expectedFallback = highlight[channel] * (1 - 128.0f / 255.0f) +
                                                rolled * (128.0f / 255.0f);
                largestError = std::fmax(largestError, std::fabs(actual[channel] - expectedFallback));
            }
            Check(fallback && largestError < 3.0f / 255.0f,
                  colourMode == 0 ? "gamma material highlights match opaque fixed-function blending" :
                                    "linear material highlights match opaque scene-copy fallback");
        }
    }
}
}

void CheckShaderInstrumentation(IDirect3DDevice9* device)
{
    shader_instrumentation_checks::Run(device);
}
