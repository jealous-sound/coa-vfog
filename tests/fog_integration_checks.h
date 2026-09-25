#pragma once

#include "ps_march_low.h"
#include "ps_march_mid.h"
#include "ps_march_high.h"

struct FogIntegrationResources
{
    IDirect3DSurface9* target = nullptr;
    IDirect3DSurface9* readback = nullptr;
    IDirect3DTexture9* depth = nullptr;
    IDirect3DSurface9* previousTarget = nullptr;
    IDirect3DSurface9* previousDepth = nullptr;
    IDirect3DStateBlock9* previousState = nullptr;

    ~FogIntegrationResources()
    {
        if (previousTarget)
            previousTarget->Release();
        if (previousDepth)
            previousDepth->Release();
        if (previousState)
            previousState->Release();
        if (target)
            target->Release();
        if (readback)
            readback->Release();
        if (depth)
            depth->Release();
    }
};

bool CreateFogIntegrationResources(IDirect3DDevice9* device, FogIntegrationResources& resources)
{
    if (FAILED(device->CreateRenderTarget(8, 8, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE,
                                          &resources.target, nullptr)) ||
        FAILED(device->CreateOffscreenPlainSurface(8, 8, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM,
                                                   &resources.readback, nullptr)) ||
        FAILED(device->CreateTexture(1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &resources.depth, nullptr)) ||
        FAILED(device->CreateStateBlock(D3DSBT_ALL, &resources.previousState)))
        return false;
    D3DLOCKED_RECT locked = {};
    if (FAILED(resources.depth->LockRect(0, &locked, nullptr, 0)))
        return false;
    *static_cast<DWORD*>(locked.pBits) = 0xFFFFFFFF;
    resources.depth->UnlockRect(0);
    return SUCCEEDED(device->GetRenderTarget(0, &resources.previousTarget)) &&
           SUCCEEDED(device->GetDepthStencilSurface(&resources.previousDepth));
}

float ReadFogIntegrationOpacity(IDirect3DDevice9* device, const FogIntegrationResources& resources)
{
    if (FAILED(device->GetRenderTargetData(resources.target, resources.readback)))
        return -1.0f;
    D3DLOCKED_RECT locked = {};
    if (FAILED(resources.readback->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
        return -1.0f;
    const unsigned char* pixel = static_cast<const unsigned char*>(locked.pBits) + 4 * locked.Pitch + 4 * 4;
    const float opacity = pixel[2] / 255.0f;
    resources.readback->UnlockRect();
    return opacity;
}

void CheckShortRangeAuthoredFog()
{
    FrameInputs input = {};
    input.farClip = 200.0f;
    input.fogEnd = 150.0f;
    input.toLight[2] = 1.0f;
    Config config;
    config.maxDistance = 200.0f;
    AuthoredFog authored = {};
    authored.layerCount = 1;
    authored.coverage = 1.0f;
    authored.layers[0].density = 0.1f;
    authored.layers[0].exponent = 2.0f;
    authored.layers[0].strength = 3.0f;
    FogParams nearLayer = BuildFogParams(input, config, &authored);
    authored.layers[0].start = 3000.0f;
    FogParams farLayer = BuildFogParams(input, config, &authored);
    Check(nearLayer.layers[0].start == 0.0f && farLayer.layers[0].start == 0.0f,
          "a short fog range never moves an authored layer behind the camera");
}

void CheckWorldShadowIntegration(IDirect3DDevice9* device, const FogIntegrationResources& resources, int quality)
{
    IDirect3DTexture9* shadow = nullptr;
    bool ready = SUCCEEDED(device->CreateTexture(4, 4, 1, 0, D3DFMT_R32F, D3DPOOL_MANAGED, &shadow, nullptr));
    D3DLOCKED_RECT locked = {};
    ready = ready && SUCCEEDED(shadow->LockRect(0, &locked, nullptr, 0));
    if (ready)
    {
        for (int y = 0; y < 4; ++y)
        {
            float* row = reinterpret_cast<float*>(static_cast<unsigned char*>(locked.pBits) + y * locked.Pitch);
            for (int x = 0; x < 4; ++x)
                row[x] = x < 2 ? 0.25f : 0.75f;
        }
        shadow->UnlockRect(0);
    }
    Check(ready, "world shadow depth fixture created");
    if (!ready)
    {
        if (shadow)
            shadow->Release();
        return;
    }
    for (DWORD stage = 4; stage < 8; ++stage)
    {
        device->SetTexture(stage, shadow);
        device->SetSamplerState(stage, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        device->SetSamplerState(stage, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        device->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        device->SetSamplerState(stage, D3DSAMP_SRGBTEXTURE, FALSE);
        device->SetSamplerState(stage, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        device->SetSamplerState(stage, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    }
    struct ShadowCase
    {
        const char* name;
        float count;
        float characterX;
        float nearX;
        float middleX;
        float farX;
        float receiverDepth;
        float visibility;
    };
    const ShadowCase cases[] = {
        {"disabled preserves the unobstructed screen-space fallback", 0, -0.5f, 2, 2, 2, 0.5f, 1},
        {"off-screen character occluder blocks light", 1, -0.5f, 2, 2, 2, 0.5f, 0},
        {"receiver before the caster remains lit", 1, -0.5f, 2, 2, 2, 0.1f, 1},
        {"outside shadow depth range uses fallback", 1, -0.5f, 2, 2, 2, 1.1f, 1},
        {"four depth comparisons filter a shadow edge", 1, 0, 2, 2, 2, 0.5f, 0.5f},
        {"character map border fades without a seam", 1, -0.845f, 2, 2, 2, 0.5f, 0.5f},
        {"off-screen environment caster blocks light", 4, 2, -0.5f, 2, 2, 0.5f, 0},
        {"middle cascade covers missing near cascade", 4, 2, 2, -0.5f, 2, 0.5f, 0},
        {"far cascade covers missing nearer cascades", 4, 2, 2, 2, -0.5f, 0.5f, 0},
        {"near cascade takes precedence over coarse far depth", 4, 2, 0.5f, -0.5f, -0.5f, 0.5f, 1},
        {"cascade border blends into the next map", 4, 2, -0.845f, 0.5f, 0.5f, 0.5f, 0.5f},
    };
    const float quad[4][4] = {{-0.5f, -0.5f, 0, 1}, {7.5f, -0.5f, 0, 1},
                             {-0.5f, 7.5f, 0, 1}, {7.5f, 7.5f, 0, 1}};
    for (const ShadowCase& sample : cases)
    {
        float constants[99][4] = {};
        constants[0][2] = constants[0][3] = 8;
        constants[1][0] = constants[1][2] = constants[1][3] = 1;
        constants[2][0] = constants[2][1] = 1;
        constants[3][0] = 1.0004f;
        constants[3][1] = -0.40016f;
        constants[3][2] = 1000;
        constants[3][3] = 0.94f;
        for (int row = 0; row < 4; ++row)
            constants[4 + row][row] = 1;
        constants[8][0] = constants[8][1] = 8;
        constants[8][2] = constants[8][3] = 0.125f;
        constants[9][2] = constants[9][3] = 1;
        constants[10][0] = 1.5f;
        constants[10][1] = 0.035f;
        constants[10][2] = 1;
        constants[10][3] = 4;
        constants[11][1] = constants[11][3] = 1000;
        constants[11][2] = 850;
        constants[12][1] = 0.001f;
        constants[12][3] = 1;
        constants[14][0] = constants[14][1] = constants[14][2] = constants[14][3] = 1;
        constants[16][3] = 1;
        constants[17][0] = 1;
        constants[17][2] = 1000;
        constants[36][0] = sample.count;
        const float mapX[] = {sample.characterX, sample.nearX, sample.middleX, sample.farX};
        for (int map = 0; map < 4; ++map)
        {
            constants[37 + map * 3][3] = mapX[map];
            constants[39 + map * 3][3] = sample.receiverDepth;
            constants[49 + map][0] = constants[49 + map][1] = 0.25f;
        }
        device->SetPixelShaderConstantF(0, &constants[0][0], 99);
        device->BeginScene();
        const HRESULT draw = device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0]));
        device->EndScene();
        const float actual = ReadFogIntegrationOpacity(device, resources);
        const float expected = sample.visibility * (1.0f - std::exp(-1.0f));
        char label[160];
        std::snprintf(label, sizeof(label), "world shadow quality %d: %s", quality, sample.name);
        Check(SUCCEEDED(draw) && std::fabs(actual - expected) <= 2.0f / 255.0f, label);
    }
    for (DWORD stage = 4; stage < 8; ++stage)
        device->SetTexture(stage, nullptr);
    shadow->Release();
}

#include "screen_shadow_checks.h"

void CheckFogIntegration(IDirect3DDevice9* device)
{
    CheckShortRangeAuthoredFog();
    FogIntegrationResources resources;
    const bool ready = CreateFogIntegrationResources(device, resources);
    Check(ready, "analytic fog integration render targets created");
    if (!ready)
        return;
    const D3DVIEWPORT9 viewport = {0, 0, 8, 8, 0.0f, 1.0f};
    device->SetDepthStencilSurface(nullptr);
    device->SetRenderTarget(0, resources.target);
    device->SetViewport(&viewport);
    device->SetVertexShader(nullptr);
    device->SetFVF(D3DFVF_XYZRHW);
    device->SetTexture(0, resources.depth);
    device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    device->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);
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
    CheckScreenSpaceShadows(device, resources);
    const float quad[4][4] = {{-0.5f, -0.5f, 0, 1}, {7.5f, -0.5f, 0, 1},
                             {-0.5f, 7.5f, 0, 1}, {7.5f, 7.5f, 0, 1}};
    const BYTE* shaders[] = {g_ps_march_low, g_ps_march_mid, g_ps_march_high};
    struct Medium
    {
        const char* name;
        float start;
        float end;
        float density;
        float heightFalloff;
    };
    const Medium media[] = {{"partial boundary cell", 780.0f, 1000.0f, 0.004f, 0.0f},
                            {"two-yard layer", 851.0f, 853.0f, 0.5f, 0.0f},
                            {"height-varying two-yard layer", 851.0f, 853.0f, 0.5f, 0.001f}};
    for (int quality = 0; quality < 3; ++quality)
    {
        IDirect3DPixelShader9* shader = nullptr;
        const bool shaderReady = SUCCEEDED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(shaders[quality]),
                                                                     &shader));
        Check(shaderReady, "analytic fog integration march shader created");
        if (!shaderReady)
            continue;
        device->SetPixelShader(shader);
        for (const Medium& medium : media)
        {
            float worst = 0.0f;
            for (int sample = 0; sample < 4; ++sample)
            {
                float constants[99][4] = {};
                constants[0][2] = constants[0][3] = 8.0f;
                constants[1][0] = 1.0f;
                constants[1][1] = static_cast<float>(sample * 17);
                constants[1][2] = constants[1][3] = 1.0f;
                constants[2][0] = constants[2][1] = 1.0f;
                constants[3][0] = 1.0004f;
                constants[3][1] = -0.40016f;
                constants[3][2] = 1000.0f;
                constants[3][3] = 0.94f;
                for (int row = 0; row < 4; ++row)
                    constants[4 + row][row] = 1.0f;
                constants[8][0] = constants[8][1] = 8.0f;
                constants[8][2] = constants[8][3] = 0.125f;
                constants[9][2] = constants[9][3] = 1.0f;
                constants[11][0] = sample == 0 ? 0.0f : 1.0f;
                constants[11][1] = constants[11][3] = 1000.0f;
                constants[11][2] = 850.0f;
                constants[12][0] = medium.start;
                constants[12][1] = medium.density;
                constants[12][3] = 1.0f;
                constants[13][0] = constants[13][1] = constants[13][2] = 1.0f;
                constants[14][3] = 1.0f;
                constants[15][1] = medium.heightFalloff;
                constants[16][3] = 1.0f;
                constants[17][2] = medium.end;
                device->SetPixelShaderConstantF(0, &constants[0][0], 99);
                device->BeginScene();
                device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0]));
                device->EndScene();
                const double heightRate = medium.heightFalloff / std::sqrt(1.0 + 2.0 * 0.125 * 0.125);
                const double pathIntegral = heightRate > 0.0
                                                ? (std::exp(-heightRate * medium.start) -
                                                   std::exp(-heightRate * medium.end)) / heightRate
                                                : medium.end - medium.start;
                const float expected = static_cast<float>(1.0 - std::exp(-medium.density * pathIntegral));
                const float actual = ReadFogIntegrationOpacity(device, resources);
                worst = std::fmax(worst, std::fabs(actual - expected));
            }
            char label[160];
            std::snprintf(label, sizeof(label), "Beer-Lambert %s at quality %d, midpoint and jitter (error %.2f/255)",
                          medium.name, quality + 1, worst * 255.0f);
            Check(worst <= 2.0f / 255.0f, label);
        }
        CheckWorldShadowIntegration(device, resources, quality + 1);
        CheckBlockedSunIntegration(device, resources, quality + 1);
        shader->Release();
    }
    device->SetRenderTarget(0, resources.previousTarget);
    device->SetDepthStencilSurface(resources.previousDepth);
    resources.previousState->Apply();
}
