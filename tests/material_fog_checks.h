#pragma once

#include "fog_volume.h"

extern "C" __declspec(dllimport) int __cdecl vf_test_begin_material_fog(const MaterialFogVolume*);
extern "C" __declspec(dllimport) int __cdecl vf_test_begin_rendered_material_fog();
extern "C" __declspec(dllimport) int __cdecl vf_test_material_fog_compatible();
extern "C" __declspec(dllimport) void __cdecl vf_test_end_material_fog();

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
}
}

void CheckRuntimeMaterialFog(IDirect3DDevice9* device)
{
    material_fog_checks::Run(device);
}

void CheckRenderedMaterialFog(Harness& harness, const D3DVIEWPORT9& viewport, const std::wstring& outDir)
{
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
