#pragma once

#include "fog_volume.h"

extern "C" __declspec(dllimport) int __cdecl vf_test_begin_material_fog(const MaterialFogVolume*);
extern "C" __declspec(dllimport) int __cdecl vf_test_begin_rendered_material_fog();
extern "C" __declspec(dllimport) int __cdecl vf_test_material_fog_compatible();
extern "C" __declspec(dllimport) void __cdecl vf_test_material_fog_requested(int);
extern "C" __declspec(dllimport) const char* __cdecl vf_test_material_fog_failure();
extern "C" __declspec(dllimport) void __cdecl vf_test_end_material_fog();
extern "C" __declspec(dllimport) int __cdecl vf_test_begin_native_glare();
extern "C" __declspec(dllimport) void __cdecl vf_test_end_native_glare();

namespace material_fog_checks
{
struct Fixture
{
    shader_instrumentation_checks::Fixture surfaces;
    IDirect3DVertexShader9* vertex = nullptr;
    IDirect3DPixelShader9* pixel = nullptr;
    IDirect3DVertexBuffer9* vertices = nullptr;
    IDirect3DIndexBuffer9* indices = nullptr;
    FogMaterialShaders instrumented;
    bool stateKept = true;

    explicit Fixture(IDirect3DDevice9* device) : surfaces(device) {}

    ~Fixture()
    {
        vf_test_end_material_fog();
        IUnknown* objects[] = {vertex, pixel, vertices, indices};
        for (IUnknown* object : objects)
            if (object)
                object->Release();
    }

    bool Create()
    {
        const auto vs = shader_instrumentation_checks::Vertex(3);
        const auto ps = shader_instrumentation_checks::Pixel(3);
        std::string failure;
        IDirect3DDevice9* dev = surfaces.dev;
        if (!surfaces.Create() || !surfaces.previousDepth ||
            !InstrumentFogMaterialShaders(vs.data(), vs.size(), ps.data(), ps.size(), instrumented, failure) ||
            FAILED(dev->CreateVertexShader(vs.data(), &vertex)) ||
            FAILED(dev->CreatePixelShader(ps.data(), &pixel)) ||
            FAILED(dev->CreateVertexBuffer(4 * 4 * sizeof(float), 0, 0, D3DPOOL_MANAGED, &vertices, nullptr)) ||
            FAILED(dev->CreateIndexBuffer(4 * sizeof(WORD), 0, D3DFMT_INDEX16, D3DPOOL_MANAGED, &indices, nullptr)))
            return false;
        WORD* data = nullptr;
        if (FAILED(indices->Lock(0, 0, reinterpret_cast<void**>(&data), 0)))
            return false;
        for (WORD index = 0; index < 4; ++index)
            data[index] = index;
        return SUCCEEDED(indices->Unlock());
    }

    bool Draw(int drawKind, int mode, float viewDepth, bool mismatchedViewport, float* rgb)
    {
        IDirect3DDevice9* dev = surfaces.dev;
        const D3DVIEWPORT9 viewport = {0, 0, 8, 8, 0, 1};
        D3DVIEWPORT9 drawViewport = viewport;
        if (mismatchedViewport)
            drawViewport.Width = 7;
        dev->SetDepthStencilSurface(surfaces.previousDepth);
        dev->SetRenderTarget(0, surfaces.target);
        dev->SetViewport(&drawViewport);
        dev->SetVertexDeclaration(surfaces.declaration);
        dev->SetVertexShader(vertex);
        dev->SetPixelShader(pixel);
        dev->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, mode != 0);
        dev->SetRenderState(D3DRS_SRCBLEND, mode == 1 ? D3DBLEND_SRCALPHA :
                                           mode == 4 ? D3DBLEND_ZERO : D3DBLEND_ONE);
        dev->SetRenderState(D3DRS_DESTBLEND, mode == 0 ? D3DBLEND_ZERO : mode == 3 ? D3DBLEND_ONE :
                                            mode == 4 ? D3DBLEND_SRCCOLOR : D3DBLEND_INVSRCALPHA);
        dev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        dev->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
        dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        dev->SetRenderState(D3DRS_FOGENABLE, TRUE);
        dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        dev->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
        dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
        const float source[4] = {0.4f, 0.2f, 0.1f, 0.4f};
        dev->SetPixelShaderConstantF(0, source, 1);
        float sentinel[kFogMaterialConstantCount][4];
        for (unsigned row = 0; row < kFogMaterialConstantCount; ++row)
            for (unsigned column = 0; column < 4; ++column)
                sentinel[row][column] = 17.0f + row * 4 + column;
        const UINT sampler = instrumented.resources.sampler;
        dev->SetPixelShaderConstantF(instrumented.resources.constantBase, &sentinel[0][0], kFogMaterialConstantCount);
        dev->SetTexture(sampler, nullptr);
        dev->SetSamplerState(sampler, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR);
        dev->SetSamplerState(sampler, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
        dev->SetSamplerState(sampler, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        dev->SetSamplerState(sampler, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        dev->SetSamplerState(sampler, D3DSAMP_MAXMIPLEVEL, 0);
        dev->SetSamplerState(sampler, D3DSAMP_SRGBTEXTURE, TRUE);
        const float data[4][4] = {{-viewDepth, viewDepth, viewDepth * 0.5f, viewDepth},
                                  {viewDepth, viewDepth, viewDepth * 0.5f, viewDepth},
                                  {-viewDepth, -viewDepth, viewDepth * 0.5f, viewDepth},
                                  {viewDepth, -viewDepth, viewDepth * 0.5f, viewDepth}};
        void* lockedVertices = nullptr;
        if (FAILED(vertices->Lock(0, 0, &lockedVertices, 0)))
            return false;
        std::memcpy(lockedVertices, data, sizeof(data));
        vertices->Unlock();
        dev->SetStreamSource(0, vertices, 0, sizeof(data[0]));
        dev->SetIndices(indices);
        const MaterialFogVolume volume = {surfaces.atlas, surfaces.target, surfaces.previousDepth, viewport,
                                           9, 1, 4, 2, 2, 8, 8, false, 0};
        if (!vf_test_begin_material_fog(&volume))
            return false;
        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xFF303030, 1, 0);
        HRESULT drawn = D3DERR_INVALIDCALL;
        const WORD elements[] = {0, 1, 2, 3};
        if (drawKind == 0)
            drawn = dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
        else if (drawKind == 1)
            drawn = dev->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP, 0, 0, 4, 0, 2);
        else if (drawKind == 2)
            drawn = dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, data, sizeof(data[0]));
        else
            drawn = dev->DrawIndexedPrimitiveUP(D3DPT_TRIANGLESTRIP, 0, 4, 2, elements, D3DFMT_INDEX16,
                                                 data, sizeof(data[0]));
        dev->EndScene();
        vf_test_end_material_fog();
        IDirect3DVertexShader9* keptVertex = nullptr;
        IDirect3DPixelShader9* keptPixel = nullptr;
        IDirect3DBaseTexture9* keptTexture = nullptr;
        DWORD addressU = 0;
        DWORD addressV = 0;
        DWORD srgb = 0;
        DWORD fog = 0;
        float constants[kFogMaterialConstantCount][4] = {};
        dev->GetVertexShader(&keptVertex);
        dev->GetPixelShader(&keptPixel);
        dev->GetTexture(sampler, &keptTexture);
        dev->GetSamplerState(sampler, D3DSAMP_ADDRESSU, &addressU);
        dev->GetSamplerState(sampler, D3DSAMP_ADDRESSV, &addressV);
        dev->GetSamplerState(sampler, D3DSAMP_SRGBTEXTURE, &srgb);
        dev->GetRenderState(D3DRS_FOGENABLE, &fog);
        dev->GetPixelShaderConstantF(instrumented.resources.constantBase, &constants[0][0], kFogMaterialConstantCount);
        stateKept = stateKept && keptVertex == vertex && keptPixel == pixel && keptTexture == nullptr &&
                    addressU == D3DTADDRESS_MIRROR && addressV == D3DTADDRESS_WRAP && srgb == TRUE && fog == TRUE &&
                    std::memcmp(constants, sentinel, sizeof(constants)) == 0;
        if (keptVertex)
            keptVertex->Release();
        if (keptPixel)
            keptPixel->Release();
        if (keptTexture)
            keptTexture->Release();
        if (FAILED(drawn) || FAILED(dev->GetRenderTargetData(surfaces.target, surfaces.readback)))
            return false;
        D3DLOCKED_RECT locked = {};
        if (FAILED(surfaces.readback->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
            return false;
        const BYTE* value = static_cast<const BYTE*>(locked.pBits) + locked.Pitch * 4 + 4 * 4;
        for (unsigned channel = 0; channel < 3; ++channel)
            rgb[channel] = value[2 - channel] / 255.0f;
        surfaces.readback->UnlockRect();
        return true;
    }
};

void Run(IDirect3DDevice9* device)
{
    Fixture fixture(device);
    const bool ready = fixture.Create();
    Check(ready, "runtime material fog fixture created through the device wrapper");
    if (!ready)
        return;
    for (int mode = 0; mode < 4; ++mode)
    {
        bool rendered = true;
        float error = 0;
        for (int drawKind = 0; drawKind < 4; ++drawKind)
        {
            float actual[3] = {};
            rendered = fixture.Draw(drawKind, mode, 4, false, actual) && rendered;
            for (int channel = 0; channel < 3; ++channel)
            {
                const float source = 0.4f / (1u << channel);
                const float radiance = mode == 3 ? 0 : (64.0f / 255.0f) / (1u << channel);
                const float foreground = source * (1 - 128.0f / 255.0f) + radiance * (mode == 2 ? 0.4f : 1);
                const float expected = mode == 0 ? foreground :
                                       mode == 1 ? foreground * 0.4f + (48.0f / 255.0f) * 0.6f :
                                       foreground + (48.0f / 255.0f) * (mode == 3 ? 1.0f : 0.6f);
                error = std::fmax(error, std::fabs(actual[channel] - expected));
            }
        }
        char label[144];
        const char* modes[] = {"opaque", "alpha", "premultiplied", "additive"};
        std::snprintf(label, sizeof(label), "all four wrapped draw APIs apply own-depth fog to %s materials",
                      modes[mode]);
        Check(rendered && error <= 3.0f / 255.0f, label);
        if (!rendered || error > 3.0f / 255.0f)
            std::printf("     wrapped material render %d, error %.2f/255\n", rendered, error * 255);
    }
    Check(fixture.stateKept,
          "material interception restores shader identities, constants, texture, sampler and fog state");
    float unfogged[3] = {};
    Check(fixture.Draw(2, 0, 4, true, unfogged) && std::fabs(unfogged[0] - 0.4f) < 2.0f / 255.0f &&
              vf_test_material_fog_compatible(),
          "material fog ignores a different viewport without disabling world compatibility");
    float unsupported[3] = {};
    Check(fixture.Draw(2, 4, 4, false, unsupported) && !vf_test_material_fog_compatible(),
          "unsupported world blending switches subsequent frames to post-world fog");
    Check(std::strcmp(vf_test_material_fog_failure(), "unsupported colour blend equation") == 0,
          "material fallback exposes the exact unsupported draw reason");
    vf_test_material_fog_requested(1);
    Check(!vf_test_material_fog_compatible(), "holding Material fog enabled does not retry every frame");
    vf_test_material_fog_requested(0);
    Check(!vf_test_material_fog_compatible(), "disabling Material fog retains the recorded failure");
    vf_test_material_fog_requested(1);
    Check(vf_test_material_fog_compatible() && !vf_test_material_fog_failure()[0],
          "explicit off/on clears material fallback and its stale diagnostic");
    float retried[3] = {};
    Check(fixture.Draw(2, 0, 4, false, retried) && vf_test_material_fog_compatible() &&
              std::fabs(retried[0] - (0.4f * (1 - 128.0f / 255.0f) + 64.0f / 255.0f)) < 3.0f / 255.0f,
          "a material compatibility retry resumes own-depth fog without a device reset");
    Check(fixture.Draw(2, 4, 4, false, unsupported) && !vf_test_material_fog_compatible(),
          "unsupported blending is still rejected after an explicit retry");
}
}

void CheckRuntimeMaterialFog(IDirect3DDevice9* device)
{
    material_fog_checks::Run(device);
}

void CheckMaterialFogOpaqueParity(Harness& harness, const D3DVIEWPORT9& viewport)
{
    IDirect3DStateBlock9* state = nullptr;
    const bool stateReady = SUCCEEDED(harness.dev->CreateStateBlock(D3DSBT_ALL, &state));
    Check(stateReady, "sun-facing material fog comparison saves the engine state");
    if (!stateReady)
        return;
    Config config = {};
    config.dataMode = 0;
    config.colorSpace = 1;
    config.noiseAmount = 0;
    config.temporal = 0;
    config.godRays = 0;
    config.worldShadows = false;
    config.localLights = false;
    config.lightShafts = false;
    config.glowCompensation = true;
    float projection[16];
    EngineProjection(static_cast<float>(viewport.Width) / viewport.Height, projection);
    const Vec3 eye = Add({0, 0, 9}, kGameLikeWorldOffset);
    const Vec3 at = Add(eye, {100, 12, 0});
    float view[16];
    CameraRelativeLookAt(eye, at, view);
    FrameInputs inputs = MakeInputs(view, projection, eye, at, viewport);
    inputs.clientGlowAmount = 0.65f;
    for (int quality = 1; quality <= 3; ++quality)
    {
        config.quality = quality;
        Image pictures[2];
        bool rendered = true;
        for (unsigned material = 0; material < 2; ++material)
        {
            config.materialFog = material != 0;
            vf_test_set_config(&config);
            harness.BeginFrame();
            harness.DrawScene(eye, view, projection, viewport);
            const char* skip = "";
            rendered = vf_test_render(&inputs, &skip) && rendered;
            harness.dev->EndScene();
            pictures[material] = Capture(harness.dev);
        }
        int worst = 0;
        if (rendered && pictures[0].bgra.size() == pictures[1].bgra.size())
            for (size_t pixel = 0; pixel < pictures[0].bgra.size(); ++pixel)
                worst = (std::max)(worst, std::abs(static_cast<int>(pictures[0].bgra[pixel]) -
                                                  static_cast<int>(pictures[1].bgra[pixel])));
        else
            rendered = false;
        char label[160];
        std::snprintf(label, sizeof(label),
                      "material atlas quality %d leaves sun-facing opaque and sky fog unchanged (max %d/255)",
                      quality, worst);
        Check(rendered && worst <= 1, label);
    }
    state->Apply();
    state->Release();
}

namespace native_glare_checks
{
struct Vertex
{
    float position[4];
    float colour[4];
};

struct Fixture
{
    IDirect3DDevice9* device;
    IDirect3DStateBlock9* state = nullptr;
    IDirect3DVertexShader9* vertex = nullptr;
    IDirect3DPixelShader9* pixel = nullptr;
    IDirect3DVertexDeclaration9* declaration = nullptr;
    IDirect3DSurface9* target = nullptr;
    IDirect3DSurface9* depth = nullptr;

    explicit Fixture(IDirect3DDevice9* value) : device(value) {}
    ~Fixture()
    {
        vf_test_end_native_glare();
        if (state)
        {
            device->SetRenderTarget(0, target);
            device->SetDepthStencilSurface(depth);
            state->Apply();
        }
        IUnknown* objects[] = {state, vertex, pixel, declaration, target, depth};
        for (IUnknown* object : objects)
            if (object)
                object->Release();
    }

    bool Create()
    {
        using namespace shader_instrumentation_checks;
        std::vector<DWORD> vs = {D3DVS_VERSION(3, 0)};
        Declare(vs, D3DSPR_INPUT, 0, D3DDECLUSAGE_POSITION);
        Declare(vs, D3DSPR_INPUT, 1, D3DDECLUSAGE_COLOR);
        Declare(vs, D3DSPR_OUTPUT, 0, D3DDECLUSAGE_POSITION);
        Declare(vs, D3DSPR_OUTPUT, 1, D3DDECLUSAGE_COLOR);
        Emit(vs, D3DSIO_MOV, {Dst(D3DSPR_OUTPUT, 0), Src(D3DSPR_INPUT, 0)});
        Emit(vs, D3DSIO_MOV, {Dst(D3DSPR_OUTPUT, 1), Src(D3DSPR_INPUT, 1)});
        vs.push_back(D3DSIO_END);
        std::vector<DWORD> ps = {D3DPS_VERSION(3, 0)};
        Declare(ps, D3DSPR_INPUT, 0, D3DDECLUSAGE_COLOR);
        Emit(ps, D3DSIO_MOV, {Dst(D3DSPR_COLOROUT, 0), Src(D3DSPR_INPUT, 0)});
        ps.push_back(D3DSIO_END);
        const D3DVERTEXELEMENT9 elements[] = {
            {0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
            {0, 16, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0}, D3DDECL_END(),
        };
        return SUCCEEDED(device->CreateStateBlock(D3DSBT_ALL, &state)) &&
               SUCCEEDED(device->GetRenderTarget(0, &target)) &&
               SUCCEEDED(device->GetDepthStencilSurface(&depth)) &&
               SUCCEEDED(device->CreateVertexShader(vs.data(), &vertex)) &&
               SUCCEEDED(device->CreatePixelShader(ps.data(), &pixel)) &&
               SUCCEEDED(device->CreateVertexDeclaration(elements, &declaration));
    }

    void SetState(const D3DVIEWPORT9& viewport, bool glare)
    {
        for (DWORD stage = 0; stage < 16; ++stage)
            device->SetTexture(stage, nullptr);
        device->SetViewport(&viewport);
        device->SetVertexShader(vertex);
        device->SetPixelShader(pixel);
        device->SetVertexDeclaration(declaration);
        device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, !glare);
        device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, glare);
        device->SetRenderState(D3DRS_SRCBLEND, glare ? D3DBLEND_SRCALPHA : D3DBLEND_ONE);
        device->SetRenderState(D3DRS_DESTBLEND, glare ? D3DBLEND_ONE : D3DBLEND_ZERO);
        device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        device->SetRenderState(D3DRS_FOGENABLE, FALSE);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
        device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    }

    bool DrawOccluder()
    {
        const Vertex quad[] = {{{0.2f, 0.4f, 0.4f, 1}, {0.13f, 0.16f, 0.2f, 1}},
                               {{0.7f, 0.4f, 0.4f, 1}, {0.13f, 0.16f, 0.2f, 1}},
                               {{0.2f, -0.4f, 0.4f, 1}, {0.13f, 0.16f, 0.2f, 1}},
                               {{0.7f, -0.4f, 0.4f, 1}, {0.13f, 0.16f, 0.2f, 1}}};
        return SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(Vertex)));
    }

    bool DrawGlare(float strength = 1)
    {
        std::vector<Vertex> fan = {{{0, 0, 0.5f, 1}, {0.9f * strength, 0.6f * strength, 0.3f * strength, 0.75f}}};
        constexpr unsigned edges = 32;
        for (unsigned edge = 0; edge <= edges; ++edge)
        {
            const float angle = 2 * kPi * edge / edges;
            fan.push_back({{0.9f * std::cos(angle), 0.9f * std::sin(angle), 0.5f, 1}, {0, 0, 0, 0}});
        }
        return SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, edges, fan.data(), sizeof(Vertex)));
    }
};

int Difference(const Image& first, const Image& second)
{
    if (first.bgra.size() != second.bgra.size() || first.bgra.empty())
        return 255;
    int worst = 0;
    for (size_t index = 0; index < first.bgra.size(); ++index)
        if (index % 4 != 3)
            worst = (std::max)(worst, std::abs(static_cast<int>(first.bgra[index]) - second.bgra[index]));
    return worst;
}

void Run(Harness& harness, const D3DVIEWPORT9& viewport, const std::wstring& outDir)
{
    Fixture fixture(harness.dev);
    const bool ready = fixture.Create();
    Check(ready, "native sky glare fixture retains the colour and depth targets");
    if (!ready)
        return;
    Config config = {};
    config.dataMode = 0;
    config.noiseAmount = 0;
    config.temporal = 0;
    config.godRays = 0;
    config.worldShadows = false;
    config.localLights = false;
    config.lightShafts = false;
    config.glowCompensation = true;
    float projection[16];
    EngineProjection(static_cast<float>(viewport.Width) / viewport.Height, projection);
    const Vec3 eye = Add({0, 0, 9}, kGameLikeWorldOffset);
    const Vec3 at = Add(eye, {100, 12, 0});
    float view[16];
    CameraRelativeLookAt(eye, at, view);
    FrameInputs inputs = MakeInputs(view, projection, eye, at, viewport);
    inputs.clientGlowAmount = 0.65f;
    D3DVIEWPORT9 skyViewport = viewport;
    skyViewport.MinZ = 0.999f;
    skyViewport.MaxZ = 1;
    for (int linear = 0; linear < 2; ++linear)
        for (int fog = 0; fog < 2; ++fog)
        {
            config.colorSpace = linear;
            config.density = static_cast<float>(fog);
            Image pictures[3];
            Image native;
            DWORD samples[3] = {};
            bool rendered = true;
            bool statesKept = true;
            for (unsigned capture = 0; capture < 3; ++capture)
            {
                config.materialFog = capture != 0;
                vf_test_set_config(&config);
                harness.BeginFrame();
                fixture.SetState(viewport, false);
                rendered = fixture.DrawOccluder() && rendered;
                const char* skip = "";
                if (capture)
                    rendered = vf_test_render(&inputs, &skip) && vf_test_begin_rendered_material_fog() && rendered;
                fixture.SetState(skyViewport, true);
                if (capture == 1)
                    rendered = vf_test_begin_native_glare() && rendered;
                IDirect3DQuery9* query = nullptr;
                bool queried = capture < 2 && SUCCEEDED(harness.dev->CreateQuery(D3DQUERYTYPE_OCCLUSION, &query));
                if (queried)
                    queried = SUCCEEDED(query->Issue(D3DISSUE_BEGIN));
                rendered = fixture.DrawGlare() && rendered;
                if (queried)
                    queried = SUCCEEDED(query->Issue(D3DISSUE_END));
                if (capture)
                {
                    if (capture == 1)
                        vf_test_end_native_glare();
                    vf_test_end_material_fog();
                }
                IDirect3DSurface9* target = nullptr;
                IDirect3DSurface9* depth = nullptr;
                D3DVIEWPORT9 restored = {};
                harness.dev->GetRenderTarget(0, &target);
                harness.dev->GetDepthStencilSurface(&depth);
                harness.dev->GetViewport(&restored);
                statesKept = statesKept && target == fixture.target && depth == fixture.depth &&
                             std::memcmp(&restored, &skyViewport, sizeof(restored)) == 0;
                if (target)
                    target->Release();
                if (depth)
                    depth->Release();
                if (!capture)
                {
                    native = Capture(harness.dev);
                    rendered = vf_test_render(&inputs, &skip) && rendered;
                }
                harness.dev->EndScene();
                pictures[capture] = Capture(harness.dev);
                HRESULT result = S_FALSE;
                const ULONGLONG deadline = GetTickCount64() + 2000;
                while (queried && result == S_FALSE && GetTickCount64() < deadline)
                    result = query->GetData(&samples[capture], sizeof(samples[capture]), D3DGETDATA_FLUSH);
                rendered = rendered && (capture == 2 || (queried && result == S_OK));
                if (query)
                    query->Release();
            }
            const int difference = Difference(pictures[0], pictures[1]);
            char label[168];
            std::snprintf(label, sizeof(label),
                          "captured native glare matches final fog in colour mode %d, density %d (max %d/255)",
                          linear, fog, difference);
            Check(rendered && difference <= 2, label);
            if (fog)
            {
                const int bypassDifference = Difference(pictures[0], pictures[2]);
                std::snprintf(label, sizeof(label),
                              "uncaptured late glare reproduces the halo regression in mode %d (max %d/255)",
                              linear, bypassDifference);
                Check(rendered && bypassDifference > 5, label);
            }
            Check(statesKept, "native glare capture restores the colour target, depth surface and sky viewport");
            Check(samples[0] > 0 && samples[0] == samples[1],
                  "native glare keeps occlusion-query timing and the original depth-tested sample count");
            std::printf("     native glare samples: reference %lu, captured %lu\n", samples[0], samples[1]);
            if (!fog)
                Check(Difference(native, pictures[1]) <= 1, "zero fog preserves the original native glare");
            if (linear && fog)
            {
                const wchar_t* names[] = {L"\\native-glare-final-fog.png", L"\\native-glare-material-fog.png",
                                           L"\\native-glare-bypassed.png"};
                for (unsigned index = 0; index < 3; ++index)
                    SavePng(outDir + names[index], pictures[index].w, pictures[index].h, pictures[index].bgra);
            }
        }
    const D3DCOLOR previousSky = harness.clearColor;
    harness.clearColor = 0xFF202830;
    config.materialFog = true;
    config.colorSpace = 1;
    config.density = 1;
    Image combined[2];
    unsigned combinedIndex = 0;
    for (float strength : {1.0f, 4.0f})
    {
        config.godRays = strength;
        vf_test_set_config(&config);
        harness.BeginFrame();
        fixture.SetState(viewport, false);
        bool rendered = fixture.DrawOccluder();
        const char* skip = "";
        rendered = vf_test_render(&inputs, &skip) && vf_test_begin_rendered_material_fog() && rendered;
        fixture.SetState(skyViewport, true);
        rendered = vf_test_begin_native_glare() && rendered;
        IDirect3DQuery9* query = nullptr;
        bool queried = SUCCEEDED(harness.dev->CreateQuery(D3DQUERYTYPE_OCCLUSION, &query));
        if (queried)
            queried = SUCCEEDED(query->Issue(D3DISSUE_BEGIN));
        rendered = fixture.DrawGlare(0.25f) && rendered;
        if (queried)
            queried = SUCCEEDED(query->Issue(D3DISSUE_END));
        vf_test_end_native_glare();
        vf_test_end_material_fog();
        harness.dev->EndScene();
        const Image picture = Capture(harness.dev);
        combined[combinedIndex++] = picture;
        DWORD samples = 0;
        HRESULT result = S_FALSE;
        const ULONGLONG deadline = GetTickCount64() + 2000;
        while (queried && result == S_FALSE && GetTickCount64() < deadline)
            result = query->GetData(&samples, sizeof(samples), D3DGETDATA_FLUSH);
        if (query)
            query->Release();
        float peak = 0;
        for (size_t index = 0; index < picture.bgra.size(); ++index)
            if (index % 4 != 3)
            {
                const float value = picture.bgra[index] / 255.0f;
                peak = std::fmax(peak, value + inputs.clientGlowAmount * value * value);
            }
        IDirect3DSurface9* target = nullptr;
        IDirect3DSurface9* depth = nullptr;
        harness.dev->GetRenderTarget(0, &target);
        harness.dev->GetDepthStencilSurface(&depth);
        const bool restored = target == fixture.target && depth == fixture.depth;
        if (target)
            target->Release();
        if (depth)
            depth->Release();
        char label[160];
        std::snprintf(label, sizeof(label),
                      "native glare and god rays %.0f preserve queries and bounded glow (peak %.3f)", strength, peak);
        Check(rendered && restored && queried && result == S_OK && samples > 0 && peak <= 1.01f, label);
    }
    Check(Difference(combined[0], combined[1]) > 1,
          "god-ray strength remains visible when native glare capture is active");
    harness.clearColor = previousSky;
}
}

void CheckRenderedMaterialFog(Harness& harness, const D3DVIEWPORT9& viewport, const std::wstring& outDir)
{
    CheckMaterialFogOpaqueParity(harness, viewport);
    native_glare_checks::Run(harness, viewport, outDir);
    IDirect3DDevice9* device = harness.dev;
    IDirect3DVertexShader9* vertex = nullptr;
    IDirect3DPixelShader9* pixel = nullptr;
    IDirect3DVertexDeclaration9* declaration = nullptr;
    IDirect3DStateBlock9* state = nullptr;
    const auto vs = shader_instrumentation_checks::Vertex(3);
    const auto ps = shader_instrumentation_checks::Pixel(3);
    const D3DVERTEXELEMENT9 elements[] = {
        {0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0}, D3DDECL_END(),
    };
    bool ready = SUCCEEDED(device->CreateVertexShader(vs.data(), &vertex)) &&
                 SUCCEEDED(device->CreatePixelShader(ps.data(), &pixel)) &&
                 SUCCEEDED(device->CreateVertexDeclaration(elements, &declaration)) &&
                 SUCCEEDED(device->CreateStateBlock(D3DSBT_ALL, &state));
    Check(ready && vf_test_material_fog_compatible(), "device reset restores material fog compatibility");
    Config config = {};
    config.dataMode = 0;
    config.noiseAmount = 0;
    config.groundFog = 0;
    config.sunScatter = 0;
    config.worldShadows = false;
    config.localLights = false;
    config.lightShafts = false;
    config.glowCompensation = false;
    config.temporal = 0;
    config.maxDistance = 800;
    float projection[16];
    EngineProjection(static_cast<float>(viewport.Width) / viewport.Height, projection);
    const Vec3 eye = {0, 0, 9};
    const Vec3 at = {1, 0, 9};
    float view[16];
    CameraRelativeLookAt(eye, at, view);
    FrameInputs inputs = MakeInputs(view, projection, eye, at, viewport);
    inputs.fogStart = 30;
    inputs.fogEnd = inputs.zoneFogDistance = 500;
    inputs.fogColor = 0xFF6389A3;
    auto drawQuad = [&](float x0, float y0, float x1, float y1, float depth, const float* colour) {
        const float clipZ = depth * (projection[10] + 1) * 0.5f + projection[14] * 0.5f;
        const float quad[4][4] = {{x0 * depth, y0 * depth, clipZ, depth},
                                 {x1 * depth, y0 * depth, clipZ, depth},
                                 {x0 * depth, y1 * depth, clipZ, depth},
                                 {x1 * depth, y1 * depth, clipZ, depth}};
        device->SetPixelShaderConstantF(0, colour, 1);
        return SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0])));
    };
    auto setMaterialState = [&]() {
        device->SetViewport(&viewport);
        device->SetVertexShader(vertex);
        device->SetPixelShader(pixel);
        device->SetVertexDeclaration(declaration);
        device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_FOGENABLE, FALSE);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
        device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    };
    Image pictures[2];
    const float depths[] = {20, 100, 400};
    const float colours[3][4] = {{0.9f, 0.24f, 0.08f, 1}, {0.18f, 0.5f, 0.85f, 0.55f},
                                {0.35f, 0.11f, 0.025f, 1}};
    for (int ownDepth = 0; ownDepth < 2 && ready; ++ownDepth)
    {
        config.materialFog = ownDepth != 0;
        vf_test_set_config(&config);
        harness.BeginFrame();
        setMaterialState();
        device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        for (int y = 0; y < 6; ++y)
            for (int x = 0; x < 8; ++x)
            {
                const float grey = (x + y) % 2 ? 0.16f : 0.28f;
                const float colour[4] = {grey, grey, grey, 1};
                ready = drawQuad(-1 + x * 0.25f, 1 - y / 3.0f, -0.75f + x * 0.25f,
                                  1 - (y + 1) / 3.0f, 780, colour) && ready;
            }
        const char* skip = "";
        if (ownDepth)
            ready = vf_test_render(&inputs, &skip) && vf_test_begin_rendered_material_fog() && ready;
        setMaterialState();
        for (int row = 0; row < 3; ++row)
        {
            device->SetRenderState(D3DRS_ALPHABLENDENABLE, row != 0);
            device->SetRenderState(D3DRS_SRCBLEND, row == 1 ? D3DBLEND_SRCALPHA : D3DBLEND_ONE);
            device->SetRenderState(D3DRS_DESTBLEND, row == 2 ? D3DBLEND_ONE : D3DBLEND_INVSRCALPHA);
            for (int column = 0; column < 3; ++column)
            {
                const float x = -0.63f + column * 0.63f;
                const float y = 0.6f - row * 0.6f;
                ready = drawQuad(x - 0.23f, y + 0.19f, x + 0.23f, y - 0.19f,
                                  depths[column], colours[row]) && ready;
            }
        }
        vf_test_end_material_fog();
        if (!ownDepth)
            ready = vf_test_render(&inputs, &skip) && ready;
        device->EndScene();
        pictures[ownDepth] = Capture(device);
    }
    const UINT nearX = viewport.X + static_cast<UINT>(viewport.Width * 0.185f);
    const UINT alphaY = viewport.Y + viewport.Height / 2;
    float nearDifference = 0;
    if (ready)
        for (unsigned channel = 0; channel < 3; ++channel)
            nearDifference = std::fmax(nearDifference, std::fabs(
                pictures[1].At(nearX, alphaY)[channel] - pictures[0].At(nearX, alphaY)[channel]) / 255.0f);
    const bool ownDepthVisible = ready && nearDifference > 0.05f;
    Check(ownDepthVisible && vf_test_material_fog_compatible(),
          "renderer atlas fogs near alpha materials at their own depth after reset");
    if (ready)
    {
        const wchar_t* names[] = {L"\\materials-background-depth.png", L"\\materials-own-depth.png"};
        for (unsigned index = 0; index < 2; ++index)
            Check(SavePng(outDir + names[index], pictures[index].w, pictures[index].h, pictures[index].bgra),
                  "material depth comparison image saved");
    }
    if (state)
        state->Apply();
    IUnknown* objects[] = {vertex, pixel, declaration, state};
    for (IUnknown* object : objects)
        if (object)
            object->Release();
}
