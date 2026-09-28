#pragma once

#include "ps_lit_composite_low.h"
#include "ps_lit_composite_mid.h"
#include "ps_lit_composite_high.h"

namespace grazing_upsample
{
constexpr UINT kWidth = 32;
constexpr UINT kHeight = 24;
constexpr float kDensity = 0.004f;
constexpr float kDepthInfinity = 0.94f;
constexpr float kDepthInverse = -0.4f;
constexpr float kMaxFogDistance = 1000.0f;
constexpr float kHorizonBlendStart = 2000.0f;
constexpr float kFarthestGroundViewZ = 400.0f;
constexpr float kNearestGroundViewZ = 100.0f;
constexpr UINT kProbeX = 16;
constexpr UINT kProbeY = 12;
constexpr int kRampStart = 25;
constexpr int kRampStep = 2;
constexpr int kZigzag = 10;
constexpr int kFilteredOpacityLevel = 200;
constexpr UINT kLightTexels = 32;
constexpr float kLightRadius = 20.0f;
constexpr int kTransmittanceView = 2;

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
    IDirect3DTexture9* filteredFog = nullptr;
    IDirect3DTexture9* currentMarch = nullptr;
    IDirect3DTexture9* lights = nullptr;
    IDirect3DPixelShader9* shaders[3] = {};
    IDirect3DPixelShader9* litShaders[3] = {};

    explicit Fixture(IDirect3DDevice9* value) : device(value) {}

    ~Fixture()
    {
        for (DWORD stage : {0u, 1u, 4u, 8u})
            device->SetTexture(stage, nullptr);
        if (savedTarget)
            device->SetRenderTarget(0, savedTarget);
        device->SetDepthStencilSurface(savedDepth);
        if (state)
            state->Apply();
        device->SetStreamSource(0, savedStream, savedOffset, savedStride);
        IUnknown* resources[] = {state, savedTarget, savedDepth, savedStream, target, readback, depth, filteredFog,
                                 currentMarch, lights, shaders[0], shaders[1], shaders[2], litShaders[0],
                                 litShaders[1], litShaders[2]};
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
            FAILED(device->CreateTexture(kLightTexels, 1, 1, 0, D3DFMT_A32B32G32R32F, D3DPOOL_MANAGED, &lights,
                                         nullptr)))
            return false;
        const BYTE* bytecode[] = {g_ps_composite_low, g_ps_composite_mid, g_ps_composite_high};
        const BYTE* litBytecode[] = {g_ps_lit_composite_low, g_ps_lit_composite_mid, g_ps_lit_composite_high};
        for (int i = 0; i < 3; ++i)
            if (FAILED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(bytecode[i]), &shaders[i])) ||
                FAILED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(litBytecode[i]), &litShaders[i])))
                return false;
        return true;
    }
};

float InverseGroundViewZ(float pixelY)
{
    const float nearest = 1.0f / kNearestGroundViewZ;
    const float farthest = 1.0f / kFarthestGroundViewZ;
    return farthest + (nearest - farthest) * pixelY / kHeight;
}

float RayLength(float pixelX, float pixelY)
{
    const float rayX = pixelX / kWidth * 2.0f - 1.0f;
    const float rayY = 1.0f - pixelY / kHeight * 2.0f;
    return std::sqrt(rayX * rayX + rayY * rayY + 1.0f);
}

float MarchedTransmittance(UINT x, UINT y)
{
    const float viewZ = 1.0f / InverseGroundViewZ(y + 0.5f);
    return std::exp(-kDensity * std::fmin(viewZ * RayLength(x + 0.5f, y + 0.5f), kMaxFogDistance));
}

int RampOpacityLevel(UINT lowResRow, bool zigzag)
{
    return kRampStart + kRampStep * static_cast<int>(lowResRow) + (zigzag && lowResRow % 2 ? kZigzag : 0);
}

float InterpolatedRampTransmittance(UINT y, UINT scale)
{
    const float lowResRow = (y - static_cast<float>(scale / 2)) / scale;
    return 1.0f - (kRampStart + kRampStep * lowResRow) / 255.0f;
}

bool FillGround(Fixture& fixture)
{
    D3DLOCKED_RECT locked = {};
    if (FAILED(fixture.depth->LockRect(0, &locked, nullptr, 0)))
        return false;
    for (UINT y = 0; y < kHeight; ++y)
    {
        float* row = reinterpret_cast<float*>(static_cast<BYTE*>(locked.pBits) + y * locked.Pitch);
        for (UINT x = 0; x < kWidth; ++x)
            row[x] = kDepthInfinity + kDepthInverse * InverseGroundViewZ(y + 0.5f);
    }
    return SUCCEEDED(fixture.depth->UnlockRect(0));
}

template <typename OpacityLevelOfRow>
bool FillLowResolution(IDirect3DDevice9* device, UINT scale, IDirect3DTexture9*& texture, OpacityLevelOfRow level)
{
    if (texture)
        texture->Release();
    texture = nullptr;
    const UINT lowWidth = (kWidth + scale - 1) / scale;
    const UINT lowHeight = (kHeight + scale - 1) / scale;
    D3DLOCKED_RECT locked = {};
    if (FAILED(device->CreateTexture(lowWidth, lowHeight, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture,
                                     nullptr)) ||
        FAILED(texture->LockRect(0, &locked, nullptr, 0)))
        return false;
    for (UINT y = 0; y < lowHeight; ++y)
    {
        auto* row = reinterpret_cast<DWORD*>(static_cast<BYTE*>(locked.pBits) + y * locked.Pitch);
        for (UINT x = 0; x < lowWidth; ++x)
            row[x] = static_cast<DWORD>(level(y)) << 24;
    }
    return SUCCEEDED(texture->UnlockRect(0));
}

bool FillLight(Fixture& fixture, float viewZ)
{
    D3DLOCKED_RECT locked = {};
    if (FAILED(fixture.lights->LockRect(0, &locked, nullptr, 0)))
        return false;
    auto* values = static_cast<float*>(locked.pBits);
    std::memset(values, 0, kLightTexels * 4 * sizeof(float));
    values[2] = viewZ;
    values[3] = kLightRadius;
    values[4] = values[5] = values[6] = 1.0f;
    values[8] = 1.0f;
    return SUCCEEDED(fixture.lights->UnlockRect(0));
}

bool ProbeTransmittance(Fixture& fixture, UINT scale, int quality, int lightCount, bool litShader,
                        float& transmittance)
{
    IDirect3DDevice9* device = fixture.device;
    const D3DVIEWPORT9 viewport = {0, 0, kWidth, kHeight, 0, 1};
    device->SetDepthStencilSurface(nullptr);
    device->SetRenderTarget(0, fixture.target);
    device->SetViewport(&viewport);
    device->SetPixelShader((litShader ? fixture.litShaders : fixture.shaders)[quality]);
    device->SetVertexShader(nullptr);
    device->SetFVF(D3DFVF_XYZRHW);
    device->SetTexture(0, fixture.depth);
    device->SetTexture(1, fixture.filteredFog);
    device->SetTexture(4, fixture.currentMarch);
    device->SetTexture(8, fixture.lights);
    for (DWORD stage : {0u, 1u, 4u, 8u})
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
    constants[0][2] = static_cast<float>(kWidth);
    constants[0][3] = static_cast<float>(kHeight);
    constants[1][0] = static_cast<float>(scale);
    constants[1][2] = 1.0f / kWidth;
    constants[1][3] = 1.0f / kHeight;
    constants[2][0] = constants[2][1] = 1;
    constants[3][0] = kDepthInfinity;
    constants[3][1] = kDepthInverse;
    constants[3][2] = kMaxFogDistance;
    constants[3][3] = kDepthInfinity;
    for (int row = 0; row < 4; ++row)
        constants[4 + row][row] = 1;
    constants[8][0] = static_cast<float>((kWidth + scale - 1) / scale);
    constants[8][1] = static_cast<float>((kHeight + scale - 1) / scale);
    constants[8][2] = 1.0f / constants[8][0];
    constants[8][3] = 1.0f / constants[8][1];
    constants[9][2] = constants[9][3] = 1;
    constants[11][1] = kMaxFogDistance;
    constants[11][2] = kHorizonBlendStart;
    constants[11][3] = kHorizonBlendStart + kMaxFogDistance;
    constants[12][1] = kDensity;
    constants[12][3] = 1;
    constants[14][3] = 1;
    constants[16][3] = 1;
    constants[17][2] = kMaxFogDistance;
    constants[53][0] = static_cast<float>(lightCount);
    constants[96][0] = 1;
    constants[96][2] = static_cast<float>(kTransmittanceView);
    device->SetPixelShaderConstantF(0, &constants[0][0], 99);
    const float quad[4][4] = {{-0.5f, -0.5f, 0, 1}, {kWidth - 0.5f, -0.5f, 0, 1},
                             {-0.5f, kHeight - 0.5f, 0, 1}, {kWidth - 0.5f, kHeight - 0.5f, 0, 1}};
    const HRESULT begin = device->BeginScene();
    const HRESULT draw = SUCCEEDED(begin)
                             ? device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0])) : begin;
    if (SUCCEEDED(begin))
        device->EndScene();
    D3DLOCKED_RECT locked = {};
    if (FAILED(draw) || FAILED(device->GetRenderTargetData(fixture.target, fixture.readback)) ||
        FAILED(fixture.readback->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
        return false;
    const BYTE* pixel = static_cast<const BYTE*>(locked.pBits) + kProbeY * locked.Pitch + kProbeX * 4;
    transmittance = pixel[2] / 255.0f;
    return SUCCEEDED(fixture.readback->UnlockRect());
}

void CheckGrazingGroundUpsample(IDirect3DDevice9* device)
{
    Fixture fixture(device);
    const bool ready = fixture.Create() && FillGround(fixture);
    Check(ready, "grazing ground upsample fixtures created");
    if (!ready)
        return;
    struct Scene
    {
        const char* name;
        bool zigzag;
        int lightCount;
        float lightViewZ;
        bool interpolated;
    };
    const Scene scenes[] = {
        {"the current march's linear ramp is interpolated, not the filtered history or a new march", false, 0, 0.0f,
         true},
        {"a light beyond the ground leaves the interpolation", false, 1, 300.0f, true},
        {"a current march curving between the taps keeps the full-resolution march", true, 0, 0.0f, false},
        {"a light crossing the view ray keeps the full-resolution march", false, 1, 60.0f, false},
    };
    for (UINT scale : {2u, 4u})
        for (const Scene& scene : scenes)
        {
            const float expected = scene.interpolated ? InterpolatedRampTransmittance(kProbeY, scale)
                                                      : MarchedTransmittance(kProbeX, kProbeY);
            bool passed =
                FillLowResolution(device, scale, fixture.filteredFog, [](UINT) { return kFilteredOpacityLevel; }) &&
                FillLowResolution(device, scale, fixture.currentMarch,
                                  [&scene](UINT row) { return RampOpacityLevel(row, scene.zigzag); }) &&
                FillLight(fixture, scene.lightViewZ);
            float worst = 0.0f;
            for (int quality = 0; quality < 3 && passed; ++quality)
                for (bool litShader : {false, true})
                {
                    if (scene.lightCount > 0 && !litShader)
                        continue;
                    float transmittance = 0.0f;
                    passed = passed &&
                             ProbeTransmittance(fixture, scale, quality, scene.lightCount, litShader, transmittance);
                    worst = std::fmax(worst, std::fabs(transmittance - expected));
                }
            char label[192];
            std::snprintf(label, sizeof(label), "grazing ground, scale %u, all qualities: %s", scale, scene.name);
            Check(passed && worst <= 2.0f / 255.0f, label);
        }
}
}
