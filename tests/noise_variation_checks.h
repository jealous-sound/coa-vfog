#pragma once

#include "ps_density_probe.h"

bool DensityNoiseHasBalancedValues(IDirect3DVolumeTexture9* noise)
{
    D3DLOCKED_BOX locked = {};
    if (FAILED(noise->LockBox(0, &locked, nullptr, D3DLOCK_READONLY)))
        return false;
    UINT counts[256] = {};
    std::vector<BYTE> values(kDensityNoiseSize * kDensityNoiseSize * kDensityNoiseSize);
    bool valid = true;
    for (UINT z = 0; z < kDensityNoiseSize; ++z)
    {
        for (UINT y = 0; y < kDensityNoiseSize; ++y)
        {
            const auto* row = reinterpret_cast<const DWORD*>(static_cast<const BYTE*>(locked.pBits) +
                                                             z * locked.SlicePitch + y * locked.RowPitch);
            for (UINT x = 0; x < kDensityNoiseSize; ++x)
            {
                const DWORD value = row[x] & 255;
                valid = valid && row[x] == (0xFF000000u | value * 0x00010101u);
                values[x + kDensityNoiseSize * (y + kDensityNoiseSize * z)] = static_cast<BYTE>(value);
                ++counts[value];
            }
        }
    }
    valid = SUCCEEDED(noise->UnlockBox(0)) && valid;
    for (UINT count : counts)
        valid = valid && count == (kDensityNoiseSize * kDensityNoiseSize * kDensityNoiseSize) / 256;
    double variance = 0;
    double covariance[3] = {};
    for (UINT z = 0; z < kDensityNoiseSize; ++z)
    {
        for (UINT y = 0; y < kDensityNoiseSize; ++y)
        {
            for (UINT x = 0; x < kDensityNoiseSize; ++x)
            {
                const double value = values[x + kDensityNoiseSize * (y + kDensityNoiseSize * z)] - 127.5;
                const UINT adjacent[] = {
                    (x + 1) % kDensityNoiseSize + kDensityNoiseSize * (y + kDensityNoiseSize * z),
                    x + kDensityNoiseSize * ((y + 1) % kDensityNoiseSize + kDensityNoiseSize * z),
                    x + kDensityNoiseSize * (y + kDensityNoiseSize * ((z + 1) % kDensityNoiseSize))};
                variance += value * value;
                for (int axis = 0; axis < 3; ++axis)
                    covariance[axis] += value * (values[adjacent[axis]] - 127.5);
            }
        }
    }
    for (double value : covariance)
        valid = valid && std::fabs(value) < variance * 0.025;
    return valid;
}

bool RenderDensityProbe(IDirect3DDevice9* device, const FogIntegrationResources& resources,
                        const float origin[4], const float step[4], const float variation[4], float pixels[64])
{
    const float quad[4][4] = {{-0.5f, -0.5f, 0, 1}, {7.5f, -0.5f, 0, 1},
                             {-0.5f, 7.5f, 0, 1}, {7.5f, 7.5f, 0, 1}};
    device->SetPixelShaderConstantF(0, origin, 1);
    device->SetPixelShaderConstantF(1, step, 1);
    device->SetPixelShaderConstantF(78, variation, 1);
    if (FAILED(device->BeginScene()))
        return false;
    const HRESULT draw = device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0]));
    device->EndScene();
    if (FAILED(draw) || FAILED(device->GetRenderTargetData(resources.target, resources.readback)))
        return false;
    D3DLOCKED_RECT locked = {};
    if (FAILED(resources.readback->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
        return false;
    for (int y = 0; y < 8; ++y)
    {
        const auto* row = static_cast<const unsigned char*>(locked.pBits) + y * locked.Pitch;
        for (int x = 0; x < 8; ++x)
            pixels[y * 8 + x] = row[x * 4 + 2] * (2.0f / 255.0f);
    }
    resources.readback->UnlockRect();
    return true;
}

void CheckNoiseVariation(IDirect3DDevice9* device)
{
    FogIntegrationResources resources;
    IDirect3DPixelShader9* shader = nullptr;
    IDirect3DVolumeTexture9* noise = nullptr;
    const bool ready = CreateFogIntegrationResources(device, resources) &&
                       SUCCEEDED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(g_ps_density_probe),
                                                            &shader)) && CreateDensityNoise(device, &noise);
    Check(ready, "spatial density-variation shader and render targets created");
    if (!ready)
    {
        if (shader)
            shader->Release();
        return;
    }
    Check(DensityNoiseHasBalancedValues(noise),
          "density noise volume is exactly balanced with low adjacent correlation along every axis");
    const D3DVIEWPORT9 viewport = {0, 0, 8, 8, 0.0f, 1.0f};
    device->SetDepthStencilSurface(nullptr);
    device->SetRenderTarget(0, resources.target);
    device->SetViewport(&viewport);
    device->SetVertexShader(nullptr);
    device->SetFVF(D3DFVF_XYZRHW);
    device->SetPixelShader(shader);
    device->SetTexture(9, noise);
    device->SetSamplerState(9, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(9, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(9, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    device->SetSamplerState(9, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    device->SetSamplerState(9, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
    device->SetSamplerState(9, D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP);
    device->SetSamplerState(9, D3DSAMP_SRGBTEXTURE, FALSE);
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
    device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);

    const float origin[4] = {-9100.0f, -100.0f, 80.0f, 0.0f};
    const float step[4] = {5.0f, 7.0f, 0.0f, 0.0f};
    float variation[4] = {0.0f, 0.025f, 0.0f, 0.0f};
    float baseline[64] = {};
    float repeated[64] = {};
    bool disabled = RenderDensityProbe(device, resources, origin, step, variation, baseline);
    for (float value : baseline)
        disabled = disabled && std::fabs(value - 1.0f) <= 1.1f / 255.0f;
    Check(disabled, "disabled spatial variation preserves the authored density exactly before quantization");

    variation[0] = 0.15f;
    const bool stable = RenderDensityProbe(device, resources, origin, step, variation, baseline) &&
                        RenderDensityProbe(device, resources, origin, step, variation, repeated) &&
                        std::memcmp(baseline, repeated, sizeof(baseline)) == 0;
    Check(stable, "stationary spatial density variation produces identical GPU samples");
    const float translated[4] = {-9087.0f, -107.0f, 80.0f, 0.0f};
    variation[2] = 13.0f;
    variation[3] = -7.0f;
    bool anchored = RenderDensityProbe(device, resources, translated, step, variation, repeated);
    for (int i = 0; i < 64; ++i)
        anchored = anchored && std::fabs(baseline[i] - repeated[i]) <= 2.0f / 255.0f;
    Check(anchored, "density variation follows world coordinates and a matching wind translation");

    variation[2] = variation[3] = 0.0f;
    const float wrapped[4] = {origin[0] + kDensityNoiseSize / variation[1], origin[1], origin[2], 0};
    bool periodic = RenderDensityProbe(device, resources, wrapped, step, variation, repeated);
    for (int i = 0; i < 64; ++i)
        periodic = periodic && std::fabs(baseline[i] - repeated[i]) <= 2.0f / 255.0f;
    Check(periodic, "density noise wraps continuously at the same period used by the wind phase");

    const float largeStep[4] = {37.25f, 61.5f, 0.0f, 0.0f};
    float minimum = 2.0f;
    float maximum = 0.0f;
    double sum = 0.0;
    bool bounded = true;
    for (int slice = 0; slice < 12; ++slice)
    {
        const float sample[4] = {-10000.0f + slice * 431.0f, -6000.0f + slice * 137.0f,
                                 10.0f + slice * 57.0f, 0.0f};
        bounded = RenderDensityProbe(device, resources, sample, largeStep, variation, repeated) && bounded;
        for (float value : repeated)
        {
            minimum = std::min(minimum, value);
            maximum = std::max(maximum, value);
            sum += value;
        }
    }
    Check(bounded && minimum >= 0.85f - 2.0f / 255.0f && maximum <= 1.15f + 2.0f / 255.0f &&
              maximum - minimum > 0.05f && std::fabs(sum / 768.0 - 1.0) < 0.06,
          "world-space density samples vary within the configured bounds around a unit mean");

    const float left[4] = {39.999f, 12.3f, -5.0f, 0.0f};
    const float right[4] = {40.001f, 12.3f, -5.0f, 0.0f};
    const float fixedStep[4] = {};
    bool continuous = RenderDensityProbe(device, resources, left, fixedStep, variation, baseline) &&
                      RenderDensityProbe(device, resources, right, fixedStep, variation, repeated);
    for (int i = 0; i < 64; ++i)
        continuous = continuous && std::fabs(baseline[i] - repeated[i]) <= 2.0f / 255.0f;
    Check(continuous, "spatial density remains continuous across noise lattice boundaries");

    device->SetRenderTarget(0, resources.previousTarget);
    device->SetDepthStencilSurface(resources.previousDepth);
    resources.previousState->Apply();
    shader->Release();
    noise->Release();

    Config config;
    const FrameInputs input = InteriorFogFixture();
    const FogParams fog = BuildFogParams(input, config, nullptr);
    Check(fog.layers[0].densityVariation == 1.0f && fog.layers[1].densityVariation == 1.0f &&
              fog.layers[kDistanceFogLayer].densityVariation == 0.0f,
          "spatial variation affects scene layers while preserving the distance-fog boundary");

    ConfigStore settings;
    Config edited = settings.Get();
    edited.noiseAmount = 2.0f;
    edited.noiseScale = 0.0f;
    edited.noiseWindSpeed = -1.0f;
    settings.Apply(edited);
    Check(settings.Get().noiseAmount == 1.0f && settings.Get().noiseScale == 0.001f &&
              settings.Get().noiseWindSpeed == 0.0f && settings.HasUnsavedChanges(),
          "density-variation controls clamp to finite live ranges and track setting changes");
}
