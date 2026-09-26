#pragma once

#include "ps_composite_low.h"
#include "ps_composite_mid.h"
#include "ps_composite_high.h"

namespace silhouette_quality
{
constexpr UINT kWidth = 43;
constexpr UINT kHeight = 31;
constexpr float kDensity = 0.004f;
constexpr float kDepthInfinity = 0.94f;
constexpr float kDepthInverse = -0.4f;

struct Fixture
{
    IDirect3DDevice9* device;
    IDirect3DStateBlock9* state = nullptr;
    IDirect3DSurface9* savedTarget = nullptr;
    IDirect3DSurface9* savedDepth = nullptr;
    IDirect3DVertexBuffer9* savedStream = nullptr;
    UINT savedOffset = 0;
    UINT savedStride = 0;
    IDirect3DSurface9* target = nullptr;
    IDirect3DSurface9* readback = nullptr;
    IDirect3DTexture9* depth = nullptr;
    IDirect3DTexture9* fog = nullptr;
    IDirect3DTexture9* scene = nullptr;
    IDirect3DPixelShader9* shaders[3] = {};

    explicit Fixture(IDirect3DDevice9* value) : device(value) {}

    ~Fixture()
    {
        device->SetTexture(0, nullptr);
        device->SetTexture(1, nullptr);
        device->SetDepthStencilSurface(nullptr);
        if (savedTarget)
            device->SetRenderTarget(0, savedTarget);
        device->SetDepthStencilSurface(savedDepth);
        if (state)
            state->Apply();
        device->SetStreamSource(0, savedStream, savedOffset, savedStride);
        IUnknown* resources[] = {state, savedTarget, savedDepth, savedStream, target, readback, depth, fog, scene,
                                 shaders[0], shaders[1], shaders[2]};
        for (IUnknown* resource : resources)
            if (resource)
                resource->Release();
    }

    bool Create()
    {
        if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &state)) ||
            FAILED(device->GetRenderTarget(0, &savedTarget)) ||
            FAILED(device->GetDepthStencilSurface(&savedDepth)) ||
            FAILED(device->GetStreamSource(0, &savedStream, &savedOffset, &savedStride)) ||
            FAILED(device->CreateRenderTarget(kWidth, kHeight, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE,
                                               0, FALSE, &target, nullptr)) ||
            FAILED(device->CreateOffscreenPlainSurface(kWidth, kHeight, D3DFMT_A8R8G8B8,
                                                        D3DPOOL_SYSTEMMEM, &readback, nullptr)) ||
            FAILED(device->CreateTexture(kWidth, kHeight, 1, 0, D3DFMT_R32F, D3DPOOL_MANAGED, &depth, nullptr)) ||
            FAILED(device->CreateTexture(1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &scene, nullptr)))
            return false;
        D3DLOCKED_RECT locked = {};
        if (FAILED(scene->LockRect(0, &locked, nullptr, 0)))
            return false;
        *static_cast<DWORD*>(locked.pBits) = 0xFF808080;
        if (FAILED(scene->UnlockRect(0)))
            return false;
        const BYTE* bytecode[] = {g_ps_composite_low, g_ps_composite_mid, g_ps_composite_high};
        for (int i = 0; i < 3; ++i)
            if (FAILED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(bytecode[i]), &shaders[i])))
                return false;
        return true;
    }

    bool SetScale(const D3DVIEWPORT9& viewport, UINT scale)
    {
        device->SetTexture(1, nullptr);
        if (fog)
            fog->Release();
        fog = nullptr;
        const UINT lowWidth = (viewport.Width + scale - 1) / scale;
        const UINT lowHeight = (viewport.Height + scale - 1) / scale;
        return SUCCEEDED(device->CreateTexture(lowWidth, lowHeight, 1, 0, D3DFMT_A8R8G8B8,
                                                D3DPOOL_MANAGED, &fog, nullptr));
    }
};

float RayLength(UINT x, UINT y, const D3DVIEWPORT9& viewport)
{
    const float rayX = (x + 0.5f - viewport.X) / viewport.Width * 2.0f - 1.0f;
    const float rayY = 1.0f - (y + 0.5f - viewport.Y) / viewport.Height * 2.0f;
    return std::sqrt(rayX * rayX + rayY * rayY + 1.0f);
}

float Transmission(float depth, float rayLength)
{
    const bool sky = depth >= 0.99903f;
    const float viewZ = depth > kDepthInfinity ? 1000.0f : kDepthInverse / (depth - kDepthInfinity);
    const float skyDensity = sky ? std::exp(-2.0f / rayLength) : 1.0f;
    return std::exp(-kDensity * std::fmin(viewZ * rayLength, 1000.0f) * skyDensity);
}

bool FillFixture(Fixture& fixture, const D3DVIEWPORT9& viewport, UINT scale, bool horizontal, UINT strip,
                  float backgroundDepth, float stripDepth)
{
    D3DLOCKED_RECT locked = {};
    if (FAILED(fixture.depth->LockRect(0, &locked, nullptr, 0)))
        return false;
    for (UINT y = 0; y < kHeight; ++y)
    {
        float* row = reinterpret_cast<float*>(static_cast<BYTE*>(locked.pBits) + y * locked.Pitch);
        for (UINT x = 0; x < kWidth; ++x)
            row[x] = (horizontal ? y == viewport.Y + strip : x == viewport.X + strip)
                         ? stripDepth : backgroundDepth;
    }
    if (FAILED(fixture.depth->UnlockRect(0)) || FAILED(fixture.fog->LockRect(0, &locked, nullptr, 0)))
        return false;
    const UINT lowWidth = (viewport.Width + scale - 1) / scale;
    const UINT lowHeight = (viewport.Height + scale - 1) / scale;
    for (UINT y = 0; y < lowHeight; ++y)
    {
        auto* row = reinterpret_cast<DWORD*>(static_cast<BYTE*>(locked.pBits) + y * locked.Pitch);
        const UINT pixelY = viewport.Y + (std::min<UINT>)(y * scale + scale / 2, viewport.Height - 1);
        for (UINT x = 0; x < lowWidth; ++x)
        {
            const UINT pixelX = viewport.X + (std::min<UINT>)(x * scale + scale / 2, viewport.Width - 1);
            const float rawDepth = (horizontal ? pixelY == viewport.Y + strip : pixelX == viewport.X + strip)
                                       ? stripDepth : backgroundDepth;
            const float opacity = 1.0f - Transmission(rawDepth, RayLength(pixelX, pixelY, viewport));
            const DWORD channel = static_cast<DWORD>(std::lround(opacity * 255.0f));
            row[x] = channel * 0x01010101u;
        }
    }
    return SUCCEEDED(fixture.fog->UnlockRect(0));
}

bool Draw(Fixture& fixture, const D3DVIEWPORT9& viewport, UINT scale, int quality, float glow = -1.0f)
{
    IDirect3DDevice9* device = fixture.device;
    device->SetDepthStencilSurface(nullptr);
    device->SetRenderTarget(0, fixture.target);
    device->SetViewport(&viewport);
    device->SetPixelShader(fixture.shaders[quality]);
    device->SetVertexShader(nullptr);
    device->SetFVF(D3DFVF_XYZRHW);
    device->SetTexture(0, fixture.depth);
    device->SetTexture(1, fixture.fog);
    device->SetTexture(3, fixture.scene);
    for (DWORD stage : {0u, 1u, 3u})
    {
        device->SetSamplerState(stage, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        device->SetSamplerState(stage, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        device->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        device->SetSamplerState(stage, D3DSAMP_SRGBTEXTURE, FALSE);
        device->SetSamplerState(stage, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        device->SetSamplerState(stage, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    }
    device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
    device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    float constants[99][4] = {};
    constants[0][0] = static_cast<float>(viewport.X);
    constants[0][1] = static_cast<float>(viewport.Y);
    constants[0][2] = static_cast<float>(viewport.Width);
    constants[0][3] = static_cast<float>(viewport.Height);
    constants[1][0] = static_cast<float>(scale);
    constants[1][2] = 1.0f / kWidth;
    constants[1][3] = 1.0f / kHeight;
    constants[2][0] = constants[2][1] = 1;
    constants[3][0] = kDepthInfinity;
    constants[3][1] = kDepthInverse;
    constants[3][2] = 1000;
    constants[3][3] = kDepthInfinity;
    for (int row = 0; row < 4; ++row)
        constants[4 + row][row] = 1;
    constants[8][0] = static_cast<float>((viewport.Width + scale - 1) / scale);
    constants[8][1] = static_cast<float>((viewport.Height + scale - 1) / scale);
    constants[8][2] = 1.0f / constants[8][0];
    constants[8][3] = 1.0f / constants[8][1];
    constants[9][2] = constants[9][3] = 1;
    constants[11][1] = 1000;
    constants[11][2] = 2000;
    constants[11][3] = 3000;
    constants[12][1] = kDensity;
    constants[12][3] = 1;
    constants[13][0] = constants[13][1] = constants[13][2] = 1;
    constants[14][3] = 1;
    constants[16][3] = 1;
    constants[17][1] = 2;
    constants[17][2] = 1000;
    constants[96][0] = 1;
    constants[96][2] = glow >= 0 ? 0.0f : 2.0f;
    constants[96][3] = glow >= 0 ? 1.0f : 0.0f;
    constants[98][3] = (std::max)(glow, 0.0f);
    device->SetPixelShaderConstantF(0, &constants[0][0], 99);
    const float left = viewport.X - 0.5f;
    const float right = viewport.X + viewport.Width - 0.5f;
    const float top = viewport.Y - 0.5f;
    const float bottom = viewport.Y + viewport.Height - 0.5f;
    const float quad[4][4] = {{left, top, 0, 1}, {right, top, 0, 1},
                             {left, bottom, 0, 1}, {right, bottom, 0, 1}};
    const HRESULT begin = device->BeginScene();
    const HRESULT draw = SUCCEEDED(begin)
                             ? device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0])) : begin;
    if (SUCCEEDED(begin))
        device->EndScene();
    return SUCCEEDED(draw) && SUCCEEDED(device->GetRenderTargetData(fixture.target, fixture.readback));
}

void CheckSilhouettes(IDirect3DDevice9* device)
{
    Fixture fixture(device);
    const bool ready = fixture.Create();
    Check(ready, "thin silhouette composite fixtures created");
    if (!ready)
        return;
    const D3DVIEWPORT9 viewports[] = {{0, 0, 32, 24, 0, 1}, {3, 2, 29, 23, 0, 1}};
    struct Scene
    {
        const char* name;
        bool horizontal;
        float background;
        float strip;
    };
    const float nearDepth = kDepthInfinity + kDepthInverse / 20.0f;
    const float farDepth = kDepthInfinity + kDepthInverse / 400.0f;
    const Scene scenes[] = {{"one-pixel foreground column", false, farDepth, nearDepth},
                            {"one-pixel foreground row", true, farDepth, nearDepth},
                            {"one-pixel background gap", false, nearDepth, farDepth},
                            {"sky beside distant terrain", false, 0.9985f, 1.0f},
                            {"distant terrain beside sky", true, 1.0f, 0.9985f}};
    for (const D3DVIEWPORT9& viewport : viewports)
        for (UINT scale : {2u, 4u})
        {
            const bool scaleReady = fixture.SetScale(viewport, scale);
            Check(scaleReady, "thin silhouette low-resolution fog texture created");
            if (!scaleReady)
                continue;
            for (const Scene& scene : scenes)
            {
                UINT strip = (scene.horizontal ? viewport.Height : viewport.Width) / 2;
                if (strip % scale == scale / 2)
                    --strip;
                const UINT x = viewport.X + (scene.horizontal ? viewport.Width / 2 : strip);
                const UINT y = viewport.Y + (scene.horizontal ? strip : viewport.Height / 2);
                const float expected = Transmission(scene.strip, RayLength(x, y, viewport));
                bool passed = FillFixture(fixture, viewport, scale, scene.horizontal, strip,
                                          scene.background, scene.strip);
                float worst = 0;
                for (int quality = 0; quality < 3 && passed; ++quality)
                {
                    D3DLOCKED_RECT locked = {};
                    passed = Draw(fixture, viewport, scale, quality) &&
                             SUCCEEDED(fixture.readback->LockRect(&locked, nullptr, D3DLOCK_READONLY));
                    if (!passed)
                        break;
                    const BYTE* pixel = static_cast<const BYTE*>(locked.pBits) + y * locked.Pitch + x * 4;
                    worst = std::fmax(worst, std::fabs(pixel[2] / 255.0f - expected));
                    fixture.readback->UnlockRect();
                }
                char label[192];
                std::snprintf(label, sizeof(label), "all qualities %s, scale %u, viewport %ux%u+%u+%u",
                              scene.name, scale, viewport.Width, viewport.Height, viewport.X, viewport.Y);
                Check(passed && worst <= 2.0f / 255.0f, label);
            }
        }
    const D3DVIEWPORT9 viewport = {0, 0, 32, 24, 0, 1};
    bool glowReady = fixture.SetScale(viewport, 2) &&
                     FillFixture(fixture, viewport, 2, false, 0, farDepth, farDepth);
    Check(glowReady, "small glow continuity fixture created");
    for (int quality = 0; quality < 3 && glowReady; ++quality)
    {
        float colours[2] = {};
        bool passed = true;
        for (int sample = 0; sample < 2; ++sample)
        {
            D3DLOCKED_RECT locked = {};
            passed = passed && Draw(fixture, viewport, 2, quality, sample == 0 ? 0.0f : 1e-9f) &&
                     SUCCEEDED(fixture.readback->LockRect(&locked, nullptr, D3DLOCK_READONLY));
            if (!passed)
                break;
            const BYTE* pixel = static_cast<const BYTE*>(locked.pBits) + 12 * locked.Pitch + 16 * 4;
            colours[sample] = pixel[2] / 255.0f;
            fixture.readback->UnlockRect();
        }
        char label[96];
        std::snprintf(label, sizeof(label), "quality %d tiny positive client glow stays continuous", quality + 1);
        Check(passed && colours[0] > 0.5f && std::fabs(colours[0] - colours[1]) <= 1.0f / 255.0f, label);
    }
}
}
