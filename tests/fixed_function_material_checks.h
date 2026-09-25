#pragma once

#include "fixed_function_material.h"

namespace fixed_function_material_checks
{
constexpr UINT kSize = 8;

struct Attributes
{
    DWORD diffuse;
    float uv[2];
};

D3DMATRIX Identity()
{
    D3DMATRIX matrix = {};
    matrix._11 = matrix._22 = matrix._33 = matrix._44 = 1;
    return matrix;
}

struct Fixture
{
    shader_instrumentation_checks::Fixture surfaces;
    IDirect3DVertexDeclaration9* declaration = nullptr;
    IDirect3DVertexBuffer9* positions = nullptr;
    IDirect3DVertexBuffer9* attributes = nullptr;
    IDirect3DTexture9* textures[2] = {};
    IDirect3DVertexBuffer9* previousStream = nullptr;
    UINT previousOffset = 0;
    UINT previousStride = 0;

    explicit Fixture(IDirect3DDevice9* device) : surfaces(device) {}

    ~Fixture()
    {
        surfaces.dev->SetStreamSource(1, previousStream, previousOffset, previousStride);
        IUnknown* resources[] = {declaration, positions, attributes, textures[0], textures[1], previousStream};
        for (IUnknown* resource : resources)
            if (resource)
                resource->Release();
    }

    bool Create()
    {
        IDirect3DDevice9* device = surfaces.dev;
        const D3DVERTEXELEMENT9 elements[] = {
            {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
            {1, 0, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
            {1, 4, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0}, D3DDECL_END()};
        if (!surfaces.Create() ||
            FAILED(device->GetStreamSource(1, &previousStream, &previousOffset, &previousStride)) ||
            FAILED(device->CreateVertexDeclaration(elements, &declaration)) ||
            FAILED(device->CreateVertexBuffer(4 * 3 * sizeof(float), 0, 0, D3DPOOL_MANAGED, &positions, nullptr)) ||
            FAILED(device->CreateVertexBuffer(4 * sizeof(Attributes), 0, 0, D3DPOOL_MANAGED, &attributes, nullptr)))
            return false;
        const Attributes data[] = {{0xC080C040, {0, 0}}, {0xA0C06090, {1, 0}},
                                    {0xE03090C0, {0, 1}}, {0x9070B050, {1, 1}}};
        void* locked = nullptr;
        if (FAILED(attributes->Lock(0, 0, &locked, 0)))
            return false;
        std::memcpy(locked, data, sizeof(data));
        if (FAILED(attributes->Unlock()))
            return false;
        for (int stage = 0; stage < 2; ++stage)
        {
            if (FAILED(device->CreateTexture(4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                                               &textures[stage], nullptr)))
                return false;
            D3DLOCKED_RECT pixels = {};
            if (FAILED(textures[stage]->LockRect(0, &pixels, nullptr, 0)))
                return false;
            for (UINT y = 0; y < 4; ++y)
            {
                auto* row = reinterpret_cast<DWORD*>(static_cast<BYTE*>(pixels.pBits) + y * pixels.Pitch);
                for (UINT x = 0; x < 4; ++x)
                    row[x] = D3DCOLOR_ARGB(72 + x * 25 + stage * 19, 30 + x * 41 + stage * 17,
                                            40 + y * 43 + stage * 13, 180 - x * 19 - y * 11 - stage * 21);
            }
            if (FAILED(textures[stage]->UnlockRect(0)))
                return false;
        }
        return SetDepth(1) && SetFog(false);
    }

    bool SetDepth(float depth)
    {
        const float data[][3] = {{-depth, depth, depth}, {depth, depth, depth},
                                  {-depth, -depth, depth}, {depth, -depth, depth}};
        void* locked = nullptr;
        if (FAILED(positions->Lock(0, 0, &locked, 0)))
            return false;
        std::memcpy(locked, data, sizeof(data));
        return SUCCEEDED(positions->Unlock());
    }

    bool SetFog(bool enabled)
    {
        D3DLOCKED_RECT locked = {};
        if (FAILED(surfaces.atlas->LockRect(0, &locked, nullptr, 0)))
            return false;
        for (UINT y = 0; y < 8; ++y)
        {
            auto* row = reinterpret_cast<DWORD*>(static_cast<BYTE*>(locked.pBits) + y * locked.Pitch);
            for (UINT x = 0; x < 8; ++x)
            {
                const UINT slice = enabled ? x / 4 + (y / 4) * 2 : 0;
                row[x] = D3DCOLOR_ARGB(slice * 64, slice * 32, slice * 16, slice * 8);
            }
        }
        return SUCCEEDED(surfaces.atlas->UnlockRect(0));
    }

    void Prepare()
    {
        IDirect3DDevice9* device = surfaces.dev;
        device->SetDepthStencilSurface(nullptr);
        device->SetRenderTarget(0, surfaces.target);
        const D3DVIEWPORT9 viewport = {0, 0, kSize, kSize, 0, 1};
        device->SetViewport(&viewport);
        device->SetVertexDeclaration(declaration);
        device->SetStreamSource(0, positions, 0, 3 * sizeof(float));
        device->SetStreamSource(1, attributes, 0, sizeof(Attributes));
        device->SetStreamSourceFreq(0, 1);
        device->SetStreamSourceFreq(1, 1);
        device->SetVertexShader(nullptr);
        device->SetPixelShader(nullptr);
        device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        device->SetRenderState(D3DRS_FOGENABLE, FALSE);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
        device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        device->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
        device->SetRenderState(D3DRS_LIGHTING, FALSE);
        device->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
        device->SetRenderState(D3DRS_VERTEXBLEND, D3DVBF_DISABLE);
        device->SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE);
        device->SetRenderState(D3DRS_POINTSPRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_TEXTUREFACTOR, 0xB060A0C0);
        D3DMATRIX matrix = Identity();
        device->SetTransform(D3DTS_WORLD, &matrix);
        device->SetTransform(D3DTS_VIEW, &matrix);
        matrix._33 = 0.5f;
        matrix._34 = 1;
        matrix._44 = 0;
        device->SetTransform(D3DTS_PROJECTION, &matrix);
        matrix = Identity();
        for (DWORD stage = 0; stage < 8; ++stage)
        {
            device->SetTexture(stage, stage < 2 ? textures[stage] : nullptr);
            device->SetSamplerState(stage, D3DSAMP_MINFILTER, D3DTEXF_POINT);
            device->SetSamplerState(stage, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
            device->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
            device->SetSamplerState(stage, D3DSAMP_SRGBTEXTURE, FALSE);
            device->SetSamplerState(stage, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            device->SetSamplerState(stage, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
            device->SetTextureStageState(stage, D3DTSS_COLOROP, D3DTOP_DISABLE);
            device->SetTextureStageState(stage, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
            device->SetTextureStageState(stage, D3DTSS_COLORARG0, D3DTA_TFACTOR);
            device->SetTextureStageState(stage, D3DTSS_COLORARG1, D3DTA_TEXTURE);
            device->SetTextureStageState(stage, D3DTSS_COLORARG2, D3DTA_CURRENT);
            device->SetTextureStageState(stage, D3DTSS_ALPHAARG0, D3DTA_TFACTOR);
            device->SetTextureStageState(stage, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            device->SetTextureStageState(stage, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
            device->SetTextureStageState(stage, D3DTSS_RESULTARG, D3DTA_CURRENT);
            device->SetTextureStageState(stage, D3DTSS_TEXCOORDINDEX, 0);
            device->SetTextureStageState(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
            device->SetTextureStageState(stage, D3DTSS_CONSTANT, 0x806030B0);
            device->SetTransform(static_cast<D3DTRANSFORMSTATETYPE>(D3DTS_TEXTURE0 + stage), &matrix);
        }
        device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    }

    bool Draw(std::vector<DWORD>& pixels)
    {
        IDirect3DDevice9* device = surfaces.dev;
        if (FAILED(device->BeginScene()))
            return false;
        device->Clear(0, nullptr, D3DCLEAR_TARGET, 0x1937597B, 1, 0);
        const HRESULT result = device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
        device->EndScene();
        static bool reported = false;
        if (FAILED(result) && !reported)
        {
            std::printf("     fixed-function DrawPrimitive failed: 0x%08lX\n", result);
            reported = true;
        }
        D3DLOCKED_RECT locked = {};
        if (FAILED(result) || FAILED(device->GetRenderTargetData(surfaces.target, surfaces.readback)) ||
            FAILED(surfaces.readback->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
            return false;
        pixels.resize(kSize * kSize);
        for (UINT y = 0; y < kSize; ++y)
            std::memcpy(pixels.data() + y * kSize, static_cast<const BYTE*>(locked.pBits) + y * locked.Pitch,
                         kSize * sizeof(DWORD));
        return SUCCEEDED(surfaces.readback->UnlockRect());
    }
};

float Difference(const std::vector<DWORD>& left, const std::vector<DWORD>& right)
{
    if (left.size() != right.size() || left.empty())
        return 255;
    float worst = 0;
    for (size_t pixel = 0; pixel < left.size(); ++pixel)
        for (UINT shift : {0u, 8u, 16u, 24u})
        {
            const int a = (left[pixel] >> shift) & 255;
            const int b = (right[pixel] >> shift) & 255;
            worst = (std::max)(worst, static_cast<float>(std::abs(a - b)));
        }
    return worst;
}

bool RenderGenerated(Fixture& fixture, const FixedFunctionMaterialState& state, const std::vector<DWORD>& vertex,
                      const std::vector<DWORD>& pixel, bool instrument, std::vector<DWORD>& pixels)
{
    IDirect3DDevice9* device = fixture.surfaces.dev;
    FogMaterialShaders fog;
    std::string failure;
    if (instrument && !InstrumentFogMaterialShaders(vertex.data(), vertex.size(), pixel.data(), pixel.size(),
                                                     fog, failure))
    {
        std::printf("     fixed-function fog instrumentation: %s\n", failure.c_str());
        return false;
    }
    IDirect3DVertexShader9* vs = nullptr;
    IDirect3DPixelShader9* ps = nullptr;
    const HRESULT vertexResult = device->CreateVertexShader(instrument ? fog.vertex.data() : vertex.data(), &vs);
    const HRESULT pixelResult = device->CreatePixelShader(instrument ? fog.pixel.data() : pixel.data(), &ps);
    if (FAILED(vertexResult) || FAILED(pixelResult))
    {
        static bool reported[2] = {};
        if (!reported[instrument ? 1 : 0])
        {
            std::printf("     fixed-function %s shader creation failed: VS=0x%08lX PS=0x%08lX\n",
                          instrument ? "instrumented" : "plain", vertexResult, pixelResult);
            reported[instrument ? 1 : 0] = true;
        }
        if (vs)
            vs->Release();
        if (ps)
            ps->Release();
        return false;
    }
    device->SetVertexShader(vs);
    device->SetPixelShader(ps);
    if (state.vertexConstantCount)
        device->SetVertexShaderConstantF(0, &state.vertexConstants[0][0], state.vertexConstantCount);
    if (state.pixelConstantCount)
        device->SetPixelShaderConstantF(0, &state.pixelConstants[0][0], state.pixelConstantCount);
    if (instrument)
    {
        const float constants[3][4] = {{1.0f / 9, 3, 2, 0.5f}, {0.5f, 0.0625f, 0.0625f, 1}, {0, 0, 0, 0}};
        device->SetPixelShaderConstantF(fog.resources.constantBase, &constants[0][0], 3);
        device->SetTexture(fog.resources.sampler, fixture.surfaces.atlas);
        device->SetSamplerState(fog.resources.sampler, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        device->SetSamplerState(fog.resources.sampler, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        device->SetSamplerState(fog.resources.sampler, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        device->SetSamplerState(fog.resources.sampler, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        device->SetSamplerState(fog.resources.sampler, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        device->SetSamplerState(fog.resources.sampler, D3DSAMP_SRGBTEXTURE, FALSE);
    }
    const bool drawn = fixture.Draw(pixels);
    if (instrument)
        device->SetTexture(fog.resources.sampler, nullptr);
    vs->Release();
    ps->Release();
    return drawn;
}

bool Build(Fixture& fixture, FixedFunctionMaterialState& state, std::vector<DWORD>& vertex, std::vector<DWORD>& pixel,
            bool generateVertex = true, bool generatePixel = true)
{
    std::string failure;
    const bool captured = CaptureFixedFunctionMaterial(fixture.surfaces.dev, generateVertex, generatePixel,
                                                        state, failure);
    const bool built = captured &&
                       BuildFixedFunctionMaterialShaders(state, vertex, pixel, failure);
    if (!built)
        std::printf("     fixed-function generation: %s\n", failure.c_str());
    return built;
}

void Compare(Fixture& fixture, const char* name)
{
    FixedFunctionMaterialState state;
    std::vector<DWORD> vertex;
    std::vector<DWORD> pixel;
    std::vector<DWORD> reference;
    std::vector<DWORD> generated;
    std::vector<DWORD> fogged;
    const bool built = Build(fixture, state, vertex, pixel);
    const bool native = fixture.Draw(reference);
    const bool plain = built && RenderGenerated(fixture, state, vertex, pixel, false, generated);
    const bool instrumented = built && RenderGenerated(fixture, state, vertex, pixel, true, fogged);
    const float worst = (std::max)(Difference(reference, generated), Difference(reference, fogged));
    char label[192];
    std::snprintf(label, sizeof(label), "fixed-function %s matches native and zero-fog shaders (%.1f/255)",
                  name, worst);
    Check(native && plain && instrumented && worst <= 2, label);
    if (worst > 2 && native && plain && instrumented)
    {
        const size_t centre = kSize * (kSize / 2) + kSize / 2;
        std::printf("     native=%08lX plain=%08lX fog=%08lX\n", reference[centre], generated[centre], fogged[centre]);
    }
}

void CheckOperations(Fixture& fixture)
{
    struct Operation { DWORD value; const char* name; };
    const Operation operations[] = {
        {D3DTOP_SELECTARG1, "SELECTARG1"}, {D3DTOP_SELECTARG2, "SELECTARG2"},
        {D3DTOP_MODULATE, "MODULATE"}, {D3DTOP_MODULATE2X, "MODULATE2X"}, {D3DTOP_MODULATE4X, "MODULATE4X"},
        {D3DTOP_ADD, "ADD"}, {D3DTOP_ADDSIGNED, "ADDSIGNED"}, {D3DTOP_ADDSIGNED2X, "ADDSIGNED2X"},
        {D3DTOP_SUBTRACT, "SUBTRACT"}, {D3DTOP_ADDSMOOTH, "ADDSMOOTH"},
        {D3DTOP_BLENDDIFFUSEALPHA, "BLENDDIFFUSEALPHA"}, {D3DTOP_BLENDTEXTUREALPHA, "BLENDTEXTUREALPHA"},
        {D3DTOP_BLENDFACTORALPHA, "BLENDFACTORALPHA"}, {D3DTOP_BLENDTEXTUREALPHAPM, "BLENDTEXTUREALPHAPM"},
        {D3DTOP_BLENDCURRENTALPHA, "BLENDCURRENTALPHA"}, {D3DTOP_MODULATEALPHA_ADDCOLOR, "MODULATEALPHA_ADDCOLOR"},
        {D3DTOP_MODULATECOLOR_ADDALPHA, "MODULATECOLOR_ADDALPHA"},
        {D3DTOP_MODULATEINVALPHA_ADDCOLOR, "MODULATEINVALPHA_ADDCOLOR"},
        {D3DTOP_MODULATEINVCOLOR_ADDALPHA, "MODULATEINVCOLOR_ADDALPHA"},
        {D3DTOP_MULTIPLYADD, "MULTIPLYADD"}, {D3DTOP_LERP, "LERP"}
    };
    for (const Operation& operation : operations)
    {
        fixture.Prepare();
        fixture.surfaces.dev->SetTextureStageState(0, D3DTSS_COLOROP, operation.value);
        Compare(fixture, operation.name);
    }
}

void CheckStages(Fixture& fixture)
{
    IDirect3DDevice9* device = fixture.surfaces.dev;
    fixture.Prepare();
    device->SetTexture(0, nullptr);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    Compare(fixture, "unlit multistream packed diffuse without textures");
    fixture.Prepare();
    device->SetTexture(0, nullptr);
    device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_ADD);
    device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    Compare(fixture, "null texture terminates the native default cascade");
    fixture.Prepare();
    device->SetTexture(0, nullptr);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
    device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
    device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_ADD);
    device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    Compare(fixture, "unused null COLORARG1 keeps SELECTARG2 cascade");
    fixture.Prepare();
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_SPECULAR);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_SPECULAR);
    Compare(fixture, "missing COLOR1 explicit specular argument");
    fixture.Prepare();
    device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MODULATE);
    device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    Compare(fixture, "two textured stages preserve distinct alpha");
    fixture.Prepare();
    device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_CONSTANT);
    Compare(fixture, "texture factor and per-stage constant");
    fixture.Prepare();
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_ADD);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE | D3DTA_ALPHAREPLICATE);
    device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TEXTURE | D3DTA_COMPLEMENT);
    device->SetTextureStageState(0, D3DTSS_RESULTARG, D3DTA_TEMP);
    device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_LERP);
    device->SetTextureStageState(1, D3DTSS_COLORARG0, D3DTA_TFACTOR | D3DTA_ALPHAREPLICATE);
    device->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_TEMP);
    device->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_CURRENT | D3DTA_COMPLEMENT);
    device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_LERP);
    device->SetTextureStageState(1, D3DTSS_ALPHAARG0, D3DTA_TEXTURE);
    device->SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_TEMP);
    device->SetTextureStageState(1, D3DTSS_ALPHAARG2, D3DTA_CURRENT | D3DTA_COMPLEMENT);
    Compare(fixture, "CURRENT and TEMP keep complemented and replicated arguments");
    for (DWORD flags : {DWORD(D3DTTFF_COUNT2), DWORD(D3DTTFF_COUNT3 | D3DTTFF_PROJECTED),
                        DWORD(D3DTTFF_COUNT4 | D3DTTFF_PROJECTED)})
    {
        fixture.Prepare();
        D3DMATRIX transform = Identity();
        transform._11 = 0.6f;
        transform._22 = 0.7f;
        transform._31 = 0.12f;
        transform._41 = 0.19f;
        transform._13 = 0.2f;
        transform._23 = 0.1f;
        transform._43 = 0.8f;
        transform._14 = 0.1f;
        transform._24 = 0.2f;
        transform._44 = 0.9f;
        device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, flags);
        device->SetTransform(D3DTS_TEXTURE0, &transform);
        const char* name = flags == D3DTTFF_COUNT2 ? "COUNT2 texture transform" :
                           flags == (D3DTTFF_COUNT3 | D3DTTFF_PROJECTED) ? "projected COUNT3 texture transform" :
                                                                                        "projected COUNT4 transform";
        Compare(fixture, name);
    }
    fixture.Prepare();
    D3DMATRIX transform = Identity();
    transform._11 = transform._22 = 0.25f;
    transform._41 = transform._42 = 0.5f;
    transform._43 = 1;
    device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
    device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT3 | D3DTTFF_PROJECTED);
    device->SetTransform(D3DTS_TEXTURE0, &transform);
    Compare(fixture, "camera-space position projective coordinates");
}

void CheckDepth(Fixture& fixture)
{
    bool ready = fixture.SetFog(true);
    float worst = 0;
    bool visible = false;
    for (float depth : {1.0f, 2.25f, 4.0f})
    {
        fixture.Prepare();
        FixedFunctionMaterialState state;
        std::vector<DWORD> vertex;
        std::vector<DWORD> pixel;
        std::vector<DWORD> native;
        std::vector<DWORD> fogged;
        ready = fixture.SetDepth(depth) && Build(fixture, state, vertex, pixel) && fixture.Draw(native) &&
                 RenderGenerated(fixture, state, vertex, pixel, true, fogged) && ready;
        if (!ready)
            break;
        const float slice = std::sqrt(depth);
        for (size_t index = 0; index < native.size(); ++index)
        {
            for (UINT channel = 0; channel < 4; ++channel)
            {
                const UINT shift = channel == 3 ? 24 : 16 - channel * 8;
                const float source = static_cast<float>((native[index] >> shift) & 255);
                const float actual = static_cast<float>((fogged[index] >> shift) & 255);
                const float expected = channel == 3 ? source :
                                       source * (1 - slice * 64 / 255) + slice * static_cast<float>(32u >> channel);
                worst = (std::max)(worst, std::fabs(actual - expected));
                visible = visible || std::fabs(actual - source) > 8;
            }
        }
    }
    char label[160];
    std::snprintf(label, sizeof(label), "fixed-function fog uses near/intermediate/far fragment depth (%.2f/255)",
                  worst);
    Check(ready && visible && worst <= 3, label);
    Check(fixture.SetDepth(1) && fixture.SetFog(false), "fixed-function depth fixture restored");
}

void CheckDynamicConstants(Fixture& fixture)
{
    IDirect3DDevice9* device = fixture.surfaces.dev;
    fixture.Prepare();
    device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
    device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
    FixedFunctionMaterialState first;
    FixedFunctionMaterialState changed;
    std::vector<DWORD> vertex;
    std::vector<DWORD> pixel;
    std::vector<DWORD> before;
    std::vector<DWORD> after;
    std::vector<DWORD> native;
    bool ready = Build(fixture, first, vertex, pixel) && RenderGenerated(fixture, first, vertex, pixel, true, before);
    device->SetVertexShader(nullptr);
    device->SetPixelShader(nullptr);
    device->SetRenderState(D3DRS_TEXTUREFACTOR, 0x9040D020);
    D3DMATRIX transform = Identity();
    transform._11 = 0.5f;
    transform._22 = 0.75f;
    device->SetTransform(D3DTS_TEXTURE0, &transform);
    D3DMATRIX world = Identity();
    world._41 = 0.18f;
    device->SetTransform(D3DTS_WORLD, &world);
    std::string failure;
    ready = CaptureFixedFunctionMaterial(device, true, true, changed, failure) && fixture.Draw(native) &&
             RenderGenerated(fixture, changed, vertex, pixel, true, after) && ready;
    Check(ready && first.key == changed.key && Difference(before, after) > 12 && Difference(native, after) <= 2,
          "fixed-function matrices and texture factor refresh constants without changing the shader key");
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_ADD);
    FixedFunctionMaterialState different;
    Check(CaptureFixedFunctionMaterial(device, true, true, different, failure) && different.key != changed.key,
          "fixed-function texture operations change the shader key");
}

std::vector<DWORD> NativeVertex()
{
    using namespace shader_instrumentation_checks;
    std::vector<DWORD> code = {D3DVS_VERSION(3, 0)};
    Declare(code, D3DSPR_INPUT, 0, D3DDECLUSAGE_POSITION);
    Declare(code, D3DSPR_INPUT, 1, D3DDECLUSAGE_COLOR);
    Declare(code, D3DSPR_INPUT, 2, D3DDECLUSAGE_TEXCOORD);
    Declare(code, D3DSPR_OUTPUT, 0, D3DDECLUSAGE_POSITION);
    Declare(code, D3DSPR_OUTPUT, 1, D3DDECLUSAGE_COLOR);
    Declare(code, D3DSPR_OUTPUT, 2, D3DDECLUSAGE_TEXCOORD);
    for (UINT component = 0; component < 4; ++component)
        Emit(code, D3DSIO_DP4, {Dst(D3DSPR_OUTPUT, 0, D3DSP_WRITEMASK_0 << component),
                                Src(D3DSPR_INPUT, 0), Src(D3DSPR_CONST, 80 + component)});
    Emit(code, D3DSIO_MOV, {Dst(D3DSPR_OUTPUT, 1), Src(D3DSPR_INPUT, 1)});
    Emit(code, D3DSIO_MOV, {Dst(D3DSPR_OUTPUT, 2), Src(D3DSPR_INPUT, 2)});
    code.push_back(D3DSIO_END);
    return code;
}

std::vector<DWORD> NativePixel(unsigned major = 2)
{
    using namespace shader_instrumentation_checks;
    const unsigned coordinateType = major == 3 ? D3DSPR_INPUT : D3DSPR_TEXTURE;
    const unsigned coordinate = major == 3 ? 1 : 0;
    std::vector<DWORD> code = {D3DPS_VERSION(major, 0)};
    if (major == 3)
    {
        Declare(code, D3DSPR_INPUT, 0, D3DDECLUSAGE_COLOR);
        Declare(code, coordinateType, coordinate, D3DDECLUSAGE_TEXCOORD);
    }
    else
    {
        Emit(code, D3DSIO_DCL, {0x80000000u, Dst(D3DSPR_INPUT, 0)});
        Emit(code, D3DSIO_DCL, {0x80000000u, Dst(coordinateType, coordinate)});
    }
    Emit(code, D3DSIO_DCL, {0x90000000u, Dst(D3DSPR_SAMPLER, 0)});
    Emit(code, D3DSIO_TEX, {Dst(D3DSPR_TEMP, 0), Src(coordinateType, coordinate), Src(D3DSPR_SAMPLER, 0)});
    Emit(code, D3DSIO_MUL, {Dst(D3DSPR_TEMP, 0), Src(D3DSPR_TEMP, 0), Src(D3DSPR_INPUT, 0)});
    Emit(code, D3DSIO_MOV, {Dst(D3DSPR_COLOROUT, 0), Src(D3DSPR_TEMP, 0)});
    code.push_back(D3DSIO_END);
    return code;
}

void CheckMixedShaders(Fixture& fixture)
{
    IDirect3DDevice9* device = fixture.surfaces.dev;
    const auto nativeVertex = NativeVertex();
    const auto nativePixel = NativePixel();
    IDirect3DVertexShader9* vs = nullptr;
    IDirect3DPixelShader9* ps = nullptr;
    const HRESULT vertexResult = device->CreateVertexShader(nativeVertex.data(), &vs);
    const HRESULT pixelResult = device->CreatePixelShader(nativePixel.data(), &ps);
    const bool created = SUCCEEDED(vertexResult) && SUCCEEDED(pixelResult);
    Check(created, "fixed-function mixed native shader fixtures created");
    if (!created)
        std::printf("     mixed native shader creation: VS=0x%08lX PS=0x%08lX\n", vertexResult, pixelResult);
    for (bool generateVertex : {false, true})
    {
        if (!created)
            break;
        fixture.Prepare();
        const float projection[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 0.5f, 0}, {0, 0, 1, 0}};
        device->SetVertexShaderConstantF(80, &projection[0][0], 4);
        device->SetVertexShader(generateVertex ? nullptr : vs);
        device->SetPixelShader(generateVertex ? ps : nullptr);
        FixedFunctionMaterialState state;
        std::vector<DWORD> vertex;
        std::vector<DWORD> pixel;
        std::vector<DWORD> reference;
        std::vector<DWORD> plain;
        std::vector<DWORD> generated;
        const bool native = fixture.Draw(reference);
        const bool built = Build(fixture, state, vertex, pixel, generateVertex, !generateVertex);
        if (generateVertex)
            pixel = nativePixel;
        else
            vertex = nativeVertex;
        bool raw = built;
        if (generateVertex)
            plain = reference;
        else
            raw = built && RenderGenerated(fixture, state, vertex, pixel, false, plain);
        const bool drawn = built && RenderGenerated(fixture, state, vertex, pixel, true, generated);
        const float worst = (std::max)(Difference(reference, plain), Difference(reference, generated));
        char label[192];
        std::snprintf(label, sizeof(label), "fixed-function generated %s with native %s keeps pixels (%.1f/255)",
                      generateVertex ? "vertex shader" : "pixel shader", generateVertex ? "pixel shader" :
                                                                                         "vertex shader", worst);
        Check(native && raw && drawn && worst <= 2, label);
        if (native && raw && drawn && worst > 2)
        {
            const size_t centre = kSize * (kSize / 2) + kSize / 2;
            std::printf("     mixed native=%08lX plain=%08lX fog=%08lX\n",
                          reference[centre], plain[centre], generated[centre]);
        }
    }
    if (vs)
        vs->Release();
    if (ps)
        ps->Release();
}

bool WrapperStateKept(Fixture& fixture, IDirect3DVertexShader9* expectedVertex, IDirect3DPixelShader9* expectedPixel,
                      const float (&vertexConstants)[kFixedFunctionVertexConstantCount][4],
                      const float (&pixelConstants)[kFixedFunctionPixelConstantCount][4])
{
    IDirect3DDevice9* device = fixture.surfaces.dev;
    IDirect3DVertexShader9* vertex = nullptr;
    IDirect3DPixelShader9* pixel = nullptr;
    IDirect3DVertexDeclaration9* declaration = nullptr;
    IDirect3DVertexBuffer9* streams[2] = {};
    UINT offsets[2] = {};
    UINT strides[2] = {};
    float vertexActual[kFixedFunctionVertexConstantCount][4] = {};
    float pixelActual[kFixedFunctionPixelConstantCount][4] = {};
    const bool read = SUCCEEDED(device->GetVertexShader(&vertex)) && SUCCEEDED(device->GetPixelShader(&pixel)) &&
                      SUCCEEDED(device->GetVertexDeclaration(&declaration)) &&
                      SUCCEEDED(device->GetStreamSource(0, &streams[0], &offsets[0], &strides[0])) &&
                      SUCCEEDED(device->GetStreamSource(1, &streams[1], &offsets[1], &strides[1])) &&
                      SUCCEEDED(device->GetVertexShaderConstantF(0, &vertexActual[0][0],
                                                                 kFixedFunctionVertexConstantCount)) &&
                      SUCCEEDED(device->GetPixelShaderConstantF(0, &pixelActual[0][0],
                                                                kFixedFunctionPixelConstantCount));
    const bool kept = read && vertex == expectedVertex && pixel == expectedPixel &&
                      declaration == fixture.declaration && streams[0] == fixture.positions &&
                      streams[1] == fixture.attributes && offsets[0] == 0 && offsets[1] == 0 &&
                      strides[0] == 3 * sizeof(float) && strides[1] == sizeof(Attributes) &&
                      std::memcmp(vertexActual, vertexConstants, sizeof(vertexActual)) == 0 &&
                      std::memcmp(pixelActual, pixelConstants, sizeof(pixelActual)) == 0;
    IUnknown* resources[] = {vertex, pixel, declaration, streams[0], streams[1]};
    for (IUnknown* resource : resources)
        if (resource)
            resource->Release();
    return kept;
}

void CheckWrapper(Fixture& fixture)
{
    IDirect3DDevice9* device = fixture.surfaces.dev;
    const auto nativeVertex = NativeVertex();
    const auto nativePixel = NativePixel();
    IDirect3DVertexShader9* vs = nullptr;
    IDirect3DPixelShader9* ps = nullptr;
    const bool created = fixture.SetFog(true) && fixture.SetDepth(4) && fixture.surfaces.previousDepth &&
                         SUCCEEDED(device->CreateVertexShader(nativeVertex.data(), &vs)) &&
                         SUCCEEDED(device->CreatePixelShader(nativePixel.data(), &ps));
    Check(created, "fixed-function wrapper GPU fixtures created");
    for (int mode = 0; mode < 3 && created; ++mode)
    {
        fixture.Prepare();
        device->SetDepthStencilSurface(fixture.surfaces.previousDepth);
        device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        device->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
        device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
        IDirect3DVertexShader9* originalVertex = mode == 1 ? vs : nullptr;
        IDirect3DPixelShader9* originalPixel = mode == 2 ? ps : nullptr;
        const float projection[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 0.5f, 0}, {0, 0, 1, 0}};
        device->SetVertexShaderConstantF(80, &projection[0][0], 4);
        std::vector<DWORD> firstPixels;
        FixedFunctionMaterialState firstState;
        bool ready = true;
        bool kept = true;
        bool cached = true;
        float worst = 0;
        for (int draw = 0; draw < 2; ++draw)
        {
            if (draw == 1)
            {
                device->SetRenderState(D3DRS_TEXTUREFACTOR, 0xA0C040E0);
                D3DMATRIX matrix = Identity();
                matrix._41 = 0.9f;
                matrix._22 = 0.7f;
                device->SetTransform(D3DTS_WORLD, &matrix);
            }
            device->SetVertexShader(originalVertex);
            device->SetPixelShader(originalPixel);
            FixedFunctionMaterialState state;
            std::vector<DWORD> vertex;
            std::vector<DWORD> pixel;
            std::vector<DWORD> reference;
            std::vector<DWORD> actual;
            const bool built = Build(fixture, state, vertex, pixel, !originalVertex, !originalPixel);
            if (originalVertex)
                vertex = nativeVertex;
            if (originalPixel)
                pixel = nativePixel;
            ready = built && RenderGenerated(fixture, state, vertex, pixel, true, reference) && ready;
            float vertexConstants[kFixedFunctionVertexConstantCount][4] = {};
            float pixelConstants[kFixedFunctionPixelConstantCount][4] = {};
            for (UINT row = 0; row < kFixedFunctionVertexConstantCount; ++row)
                for (UINT column = 0; column < 4; ++column)
                    vertexConstants[row][column] = 7 + row * 0.25f + column;
            for (UINT row = 0; row < kFixedFunctionPixelConstantCount; ++row)
                for (UINT column = 0; column < 4; ++column)
                    pixelConstants[row][column] = 17 + row * 0.5f + column;
            device->SetVertexShaderConstantF(0, &vertexConstants[0][0], kFixedFunctionVertexConstantCount);
            device->SetPixelShaderConstantF(0, &pixelConstants[0][0], kFixedFunctionPixelConstantCount);
            device->SetVertexShader(originalVertex);
            device->SetPixelShader(originalPixel);
            const D3DVIEWPORT9 viewport = {0, 0, kSize, kSize, 0, 1};
            const MaterialFogVolume volume = {fixture.surfaces.atlas, fixture.surfaces.target,
                                               fixture.surfaces.previousDepth, viewport, 9, 1, 4, 2, 2, 8, 8,
                                               false, 0};
            const bool begun = vf_test_begin_material_fog(&volume) != 0;
            const bool drawn = begun && fixture.Draw(actual);
            kept = WrapperStateKept(fixture, originalVertex, originalPixel, vertexConstants, pixelConstants) && kept;
            vf_test_end_material_fog();
            ready = drawn && ready;
            worst = (std::max)(worst, Difference(reference, actual));
            if (draw == 0)
            {
                firstPixels = actual;
                firstState = state;
            }
            else
                cached = firstState.key == state.key && Difference(firstPixels, actual) > 12;
        }
        const char* name = mode == 0 ? "missing VS and PS" : mode == 1 ? "missing PS" : "missing VS";
        char label[192];
        std::snprintf(label, sizeof(label), "fixed-function wrapper %s applies own-depth fog (%.1f/255)", name, worst);
        Check(ready && worst <= 2 && vf_test_material_fog_compatible() != 0, label);
        std::snprintf(label, sizeof(label), "fixed-function wrapper %s restores shaders/constants/declaration/streams",
                      name);
        Check(kept, label);
        std::snprintf(label, sizeof(label), "fixed-function wrapper %s refreshes cached matrices and texture factor",
                      name);
        Check(ready && cached, label);
    }
    if (vs)
        vs->Release();
    if (ps)
        ps->Release();
    Check(fixture.SetFog(false) && fixture.SetDepth(1), "fixed-function wrapper fixture restored");
    vf_test_material_fog_requested(0);
    vf_test_material_fog_requested(1);
}

void CheckRejections(Fixture& fixture)
{
    IDirect3DDevice9* device = fixture.surfaces.dev;
    struct State { D3DRENDERSTATETYPE key; DWORD value; const char* name; };
    const State states[] = {{D3DRS_LIGHTING, TRUE, "lighting"},
                            {D3DRS_VERTEXBLEND, D3DVBF_1WEIGHTS, "vertex skinning"},
                            {D3DRS_INDEXEDVERTEXBLENDENABLE, TRUE, "indexed vertex skinning"}};
    for (const State& rejected : states)
    {
        fixture.Prepare();
        device->SetRenderState(rejected.key, rejected.value);
        FixedFunctionMaterialState state;
        std::string failure;
        const bool captured = CaptureFixedFunctionMaterial(device, true, true, state, failure);
        char label[144];
        std::snprintf(label, sizeof(label), "fixed-function unsupported %s is rejected with a reason", rejected.name);
        Check(!captured && !failure.empty(), label);
    }
    fixture.Prepare();
    D3DMATRIX orthographic = Identity();
    device->SetTransform(D3DTS_PROJECTION, &orthographic);
    FixedFunctionMaterialState state;
    std::string failure;
    Check(!CaptureFixedFunctionMaterial(device, true, true, state, failure) && !failure.empty(),
          "fixed-function orthographic projection has no invented view depth");
    fixture.Prepare();
    device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT1);
    Check(!CaptureFixedFunctionMaterial(device, true, true, state, failure) && !failure.empty(),
          "fixed-function COUNT1 texture transform is rejected instead of changing sampling");
    fixture.Prepare();
    const auto pixelCode = NativePixel(3);
    IDirect3DPixelShader9* pixel = nullptr;
    const bool pixelCreated = SUCCEEDED(device->CreatePixelShader(pixelCode.data(), &pixel));
    device->SetPixelShader(pixel);
    Check(pixelCreated && !CaptureFixedFunctionMaterial(device, true, false, state, failure) && !failure.empty(),
          "fixed-function native pixel shader3 linkage is rejected instead of changing sampling");
    device->SetPixelShader(nullptr);
    if (pixel)
        pixel->Release();
    fixture.Prepare();
    device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACENORMAL);
    Check(!CaptureFixedFunctionMaterial(device, true, true, state, failure) && !failure.empty(),
          "fixed-function unsupported normal texgen is rejected with a reason");
    fixture.Prepare();
    device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
    Check(!CaptureFixedFunctionMaterial(device, true, true, state, failure) && !failure.empty(),
          "fixed-function transformed POSITIONT has no invented view depth");
}

void Run(IDirect3DDevice9* device)
{
    Fixture fixture(device);
    const bool ready = fixture.Create();
    Check(ready, "fixed-function multistream GPU fixtures created");
    if (!ready)
        return;
    CheckOperations(fixture);
    CheckStages(fixture);
    CheckDepth(fixture);
    CheckDynamicConstants(fixture);
    CheckMixedShaders(fixture);
    CheckWrapper(fixture);
    CheckRejections(fixture);
}
}

void CheckFixedFunctionMaterials(IDirect3DDevice9* device)
{
    fixed_function_material_checks::Run(device);
}
