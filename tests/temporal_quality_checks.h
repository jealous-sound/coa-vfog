#include "ps_temporal.h"
#include "ps_history_depth.h"
#include "ps_ray_mask.h"
#include "vs_fullscreen.h"

namespace temporal_quality
{
constexpr UINT kSize = 4;
constexpr UINT kRayDepthSize = 32;
constexpr float kFogDistance = 100.0f;
constexpr float kCurrentFog = 0.2f;
constexpr float kHistoryFog = 0.8f;
constexpr float kWeight = 0.75f;

DWORD Grey(float value)
{
    const auto channel = static_cast<DWORD>(std::lround(value * 255.0f));
    return channel * 0x01010101u;
}

DWORD PackedDepth(float viewZ, DWORD depthClass)
{
    const auto packed = static_cast<DWORD>(std::lround(viewZ / kFogDistance * 65535.0f));
    return 0xFF000000u | ((packed >> 8) << 16) | ((packed & 255u) << 8) | depthClass;
}

bool Fill(IDirect3DTexture9* texture, DWORD colour, int changedIndex = -1, DWORD changedColour = 0)
{
    D3DLOCKED_RECT locked = {};
    if (FAILED(texture->LockRect(0, &locked, nullptr, 0)))
        return false;
    for (UINT y = 0; y < kSize; ++y)
    {
        auto* row = reinterpret_cast<DWORD*>(static_cast<BYTE*>(locked.pBits) + y * locked.Pitch);
        for (UINT x = 0; x < kSize; ++x)
            row[x] = static_cast<int>(y * kSize + x) == changedIndex ? changedColour : colour;
    }
    return SUCCEEDED(texture->UnlockRect(0));
}

struct Fixture
{
    IDirect3DDevice9* dev;
    IDirect3DStateBlock9* state = nullptr;
    IDirect3DSurface9* savedTarget = nullptr;
    IDirect3DSurface9* savedDepth = nullptr;
    IDirect3DVertexBuffer9* savedStream = nullptr;
    UINT savedOffset = 0;
    UINT savedStride = 0;
    IDirect3DTexture9* inputs[4] = {};
    IDirect3DTexture9* output = nullptr;
    IDirect3DTexture9* rayDepth = nullptr;
    IDirect3DSurface9* target = nullptr;
    IDirect3DSurface9* readback = nullptr;
    IDirect3DVertexShader9* vs = nullptr;
    IDirect3DPixelShader9* ps = nullptr;
    IDirect3DPixelShader9* depthPs = nullptr;
    IDirect3DPixelShader9* rayPs = nullptr;
    IDirect3DVertexDeclaration9* declaration = nullptr;
    float reproject[16] = {};
    DWORD encodedResult = 0;

    explicit Fixture(IDirect3DDevice9* device) : dev(device) {}

    ~Fixture()
    {
        if (savedTarget)
            dev->SetRenderTarget(0, savedTarget);
        dev->SetDepthStencilSurface(savedDepth);
        if (state)
            state->Apply();
        dev->SetStreamSource(0, savedStream, savedOffset, savedStride);
        IUnknown* resources[] = {state, savedTarget, savedDepth, savedStream, inputs[0], inputs[1], inputs[2],
                                 inputs[3], output, rayDepth, target, readback, vs, ps, depthPs, rayPs, declaration};
        for (IUnknown* resource : resources)
            if (resource)
                resource->Release();
    }

    bool Create()
    {
        if (FAILED(dev->CreateStateBlock(D3DSBT_ALL, &state)))
            return false;
        dev->GetRenderTarget(0, &savedTarget);
        dev->GetDepthStencilSurface(&savedDepth);
        dev->GetStreamSource(0, &savedStream, &savedOffset, &savedStride);
        for (auto*& texture : inputs)
            if (FAILED(dev->CreateTexture(kSize, kSize, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                                          &texture, nullptr)))
                return false;
        static const D3DVERTEXELEMENT9 elements[] = {
            {0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0}, D3DDECL_END()};
        if (FAILED(dev->CreateTexture(kSize, kSize, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                      D3DPOOL_DEFAULT, &output, nullptr)) ||
            FAILED(output->GetSurfaceLevel(0, &target)) ||
            FAILED(dev->CreateOffscreenPlainSurface(kSize, kSize, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM,
                                                    &readback, nullptr)) ||
            FAILED(dev->CreateVertexShader(reinterpret_cast<const DWORD*>(g_vs_fullscreen), &vs)) ||
            FAILED(dev->CreatePixelShader(reinterpret_cast<const DWORD*>(g_ps_temporal), &ps)) ||
            FAILED(dev->CreatePixelShader(reinterpret_cast<const DWORD*>(g_ps_history_depth), &depthPs)) ||
            FAILED(dev->CreatePixelShader(reinterpret_cast<const DWORD*>(g_ps_ray_mask), &rayPs)) ||
            FAILED(dev->CreateTexture(kRayDepthSize, kRayDepthSize, 1, 0, D3DFMT_A8R8G8B8,
                                      D3DPOOL_MANAGED, &rayDepth, nullptr)) ||
            FAILED(dev->CreateVertexDeclaration(elements, &declaration)))
            return false;
        return true;
    }

    bool Reset()
    {
        std::memset(reproject, 0, sizeof(reproject));
        reproject[0] = reproject[5] = reproject[11] = 1.0f;
        if (!Fill(inputs[0], Grey(kCurrentFog), 0, Grey(0.0f)) ||
            !Fill(inputs[1], Grey(kHistoryFog)) || !Fill(inputs[2], Grey(128.0f / 255.0f)) ||
            !Fill(inputs[3], PackedDepth(1.0f, 0)))
            return false;
        D3DLOCKED_RECT locked = {};
        if (FAILED(inputs[0]->LockRect(0, &locked, nullptr, 0)))
            return false;
        auto* row = reinterpret_cast<DWORD*>(static_cast<BYTE*>(locked.pBits) + 2 * locked.Pitch);
        row[2] = Grey(1.0f);
        return SUCCEEDED(inputs[0]->UnlockRect(0));
    }

    float Draw(bool historyValid = true, float captureDepth = 0.0f)
    {
        float constants[14][4] = {
            {0, 0, kSize, kSize},
            {1, 0, 1.0f / kSize, 1.0f / kSize},
            {1, 1, 0, 0},
            {1, 128.0f / 255.0f - 1.0f, kFogDistance, 0.94f},
            {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1},
            {kSize, kSize, 1.0f / kSize, 1.0f / kSize},
            {}, {}, {}, {},
            {kWeight, historyValid ? 1.0f : 0.0f, 0, 0},
        };
        std::memcpy(constants[9], reproject, sizeof(reproject));
        if (captureDepth > 0.0f)
        {
            constants[3][1] *= captureDepth;
            constants[3][2] = 5000.0f;
        }
        dev->SetDepthStencilSurface(nullptr);
        dev->SetRenderTarget(0, target);
        D3DVIEWPORT9 viewport = {0, 0, kSize, kSize, 0.0f, 1.0f};
        dev->SetViewport(&viewport);
        dev->SetVertexShader(vs);
        dev->SetPixelShader(captureDepth > 0.0f ? depthPs : ps);
        dev->SetVertexDeclaration(declaration);
        dev->SetStreamSourceFreq(0, 1);
        dev->SetRenderState(D3DRS_ZENABLE, FALSE);
        dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
        dev->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
        dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
        dev->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
        dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        for (DWORD stage = 0; stage < 4; ++stage)
        {
            dev->SetTexture(stage, inputs[captureDepth > 0.0f && stage == 0 ? 2 : stage]);
            dev->SetSamplerState(stage, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            dev->SetSamplerState(stage, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
            dev->SetSamplerState(stage, D3DSAMP_MINFILTER, D3DTEXF_POINT);
            dev->SetSamplerState(stage, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
            dev->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
            dev->SetSamplerState(stage, D3DSAMP_SRGBTEXTURE, FALSE);
        }
        dev->SetPixelShaderConstantF(0, &constants[0][0], 14);
        return DrawConfigured();
    }

    float DrawConfigured()
    {
        const float triangle[3][4] = {{-1, -1, 0, 1}, {-1, 3, 0, 1}, {3, -1, 0, 1}};
        if (FAILED(dev->BeginScene()))
            return -1.0f;
        const HRESULT drawn = dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, triangle, sizeof(triangle[0]));
        const HRESULT ended = dev->EndScene();
        if (FAILED(drawn) || FAILED(ended) || FAILED(dev->GetRenderTargetData(target, readback)))
            return -1.0f;
        D3DLOCKED_RECT locked = {};
        if (FAILED(readback->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
            return -1.0f;
        const auto* row = reinterpret_cast<const DWORD*>(static_cast<const BYTE*>(locked.pBits) + locked.Pitch);
        encodedResult = row[1];
        const float result = static_cast<float>((encodedResult >> 16) & 255u) / 255.0f;
        readback->UnlockRect();
        return result;
    }

    float DrawRayMask(bool skyAtSample)
    {
        if (Draw(false) < 0.0f || !Fill(inputs[1], Grey(1.0f)))
            return -1.0f;
        D3DLOCKED_RECT locked = {};
        if (FAILED(rayDepth->LockRect(0, &locked, nullptr, 0)))
            return -1.0f;
        for (UINT y = 0; y < kRayDepthSize; ++y)
        {
            auto* row = reinterpret_cast<DWORD*>(static_cast<BYTE*>(locked.pBits) + y * locked.Pitch);
            for (UINT x = 0; x < kRayDepthSize; ++x)
                row[x] = Grey((y == 12) == skyAtSample ? 1.0f : 0.5f);
        }
        if (FAILED(rayDepth->UnlockRect(0)))
            return -1.0f;
        const float viewport[4] = {3, 5, 17, 19};
        const float pixelGrid[4] = {1, 0, 1.0f / kRayDepthSize, 1.0f / kRayDepthSize};
        const float mask[2][4] = {{0, 0, 1, 0}, {17.0f / kSize, 0, 1.0f / kSize, 1.0f / kSize}};
        dev->SetPixelShader(rayPs);
        dev->SetPixelShaderConstantF(0, viewport, 1);
        dev->SetPixelShaderConstantF(1, pixelGrid, 1);
        dev->SetPixelShaderConstantF(9, &mask[0][0], 2);
        dev->SetTexture(0, rayDepth);
        dev->SetTexture(1, inputs[1]);
        return DrawConfigured();
    }
};

bool NearTemporal(float actual, float expected)
{
    if (std::fabs(actual - expected) > 2.0f / 255.0f)
    {
        std::printf("     temporal value %.4f expected %.4f\n", actual, expected);
        return false;
    }
    return true;
}

bool CapturesDepth(Fixture& fixture, float viewDepth, DWORD rawDepth, DWORD expectedClass)
{
    if (!Fill(fixture.inputs[2], rawDepth) || fixture.Draw(true, viewDepth) < 0.0f)
        return false;
    const DWORD red = (fixture.encodedResult >> 16) & 255u;
    const DWORD green = (fixture.encodedResult >> 8) & 255u;
    const int depthClass = static_cast<int>(fixture.encodedResult & 255u);
    const double restoredDepth = (red * 256u + green) * (5000.0 / 65535.0);
    const double maximumError = 0.5 * 5000.0 / 65535.0;
    const bool matches = std::fabs(restoredDepth - viewDepth) <= maximumError &&
                         std::abs(depthClass - static_cast<int>(expectedClass)) <= 1;
    if (!matches)
        std::printf("     depth history %.6f yd class %d expected %.6f yd class %lu\n",
                    restoredDepth, depthClass, viewDepth, expectedClass);
    return matches;
}
}

void CheckTemporalQuality(IDirect3DDevice9* dev)
{
    using namespace temporal_quality;
    Fixture fixture(dev);
    const bool ready = fixture.Create() && fixture.Reset();
    Check(ready, "temporal regression shader resources created");
    if (!ready)
        return;
    const float accumulated = kCurrentFog * (1 - kWeight) + kHistoryFog * kWeight;
    Check(NearTemporal(fixture.Draw(), accumulated), "temporal matching surface reuses history");
    fixture.reproject[15] = 1.0f;
    Check(Fill(fixture.inputs[3], PackedDepth(2.0f, 0)) && NearTemporal(fixture.Draw(), accumulated),
          "temporal camera translation validates distance in the previous view");
    fixture.Reset();
    Check(Fill(fixture.inputs[3], PackedDepth(10.0f, 0)) && NearTemporal(fixture.Draw(), kCurrentFog),
          "temporal newly exposed geometry rejects history at another depth");
    Check(Fill(fixture.inputs[3], PackedDepth(1.0f, 255)) && NearTemporal(fixture.Draw(), kCurrentFog),
          "temporal geometry rejects sky history even at matching depth");
    fixture.Reset();
    fixture.reproject[8] = 0.25f;
    Check(Fill(fixture.inputs[1], Grey(0.4f), 6, Grey(1.0f)) &&
              Fill(fixture.inputs[3], PackedDepth(1.0f, 0), 6, PackedDepth(10.0f, 0)) &&
              NearTemporal(fixture.Draw(), kCurrentFog * (1 - kWeight) + 0.4f * kWeight),
          "temporal bilinear history excludes an occluded neighbour before filtering");
    fixture.Reset();
    Check(Fill(fixture.inputs[2], Grey(1.0f)) && Fill(fixture.inputs[3], PackedDepth(50.0f, 255)) &&
              NearTemporal(fixture.Draw(), accumulated),
          "temporal sky history remains usable without a finite surface distance");
    Check(Fill(fixture.inputs[3], PackedDepth(kFogDistance, 128)) && NearTemporal(fixture.Draw(), kCurrentFog),
          "temporal sky rejects distant terrain history");
    fixture.Reset();
    Check(NearTemporal(fixture.Draw(false), kCurrentFog), "temporal invalid history returns the current sample");
    fixture.reproject[8] = 10.0f;
    Check(NearTemporal(fixture.Draw(), kCurrentFog), "temporal off-screen reprojection returns the current sample");
    fixture.Reset();
    fixture.reproject[11] = -1.0f;
    Check(NearTemporal(fixture.Draw(), kCurrentFog),
          "temporal reprojection behind the camera returns the current sample");
    fixture.Reset();
    for (float viewDepth : {0.4f, 1.0f, 100.0f, 4999.0f})
    {
        char label[120];
        std::snprintf(label, sizeof(label), "depth history captures %.1f yd within half a 16-bit depth interval",
                      viewDepth);
        Check(CapturesDepth(fixture, viewDepth, Grey(128.0f / 255.0f), 0), label);
    }
    Check(CapturesDepth(fixture, 5000.0f, Grey(0.98f), 128),
          "depth history distinguishes distant terrain at the maximum fog distance");
    Check(CapturesDepth(fixture, 5000.0f, Grey(1.0f), 255),
          "depth history distinguishes clear sky from distant terrain");
    fixture.Reset();
    Check(NearTemporal(fixture.DrawRayMask(true), 1.0f),
          "god-ray mask samples sky at the matching odd subviewport position");
    Check(NearTemporal(fixture.DrawRayMask(false), 0.0f),
          "god-ray mask rejects geometry at the matching odd subviewport position");
}
