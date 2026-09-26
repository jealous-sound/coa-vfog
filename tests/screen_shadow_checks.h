#pragma once

#include "ps_shadow_probe.h"

void CheckBlockedSunIntegration(IDirect3DDevice9* device, const FogIntegrationResources& resources, int quality,
                                bool foreground = false)
{
    IDirect3DTexture9* depth = nullptr;
    bool ready = SUCCEEDED(device->CreateTexture(64, 64, 1, 0, D3DFMT_R32F, D3DPOOL_MANAGED, &depth, nullptr));
    D3DLOCKED_RECT locked = {};
    ready = ready && SUCCEEDED(depth->LockRect(0, &locked, nullptr, 0));
    if (ready)
    {
        for (int y = 0; y < 64; ++y)
        {
            float* row = reinterpret_cast<float*>(static_cast<unsigned char*>(locked.pBits) + y * locked.Pitch);
            for (int x = 0; x < 64; ++x)
                row[x] = 0.940376f - 0.3761504f / (foreground && x < 35 ? 2 : 120);
        }
        depth->UnlockRect(0);
    }
    Check(ready, "blocked sunlight integration depth fixture created");
    if (!ready)
    {
        if (depth)
            depth->Release();
        return;
    }
    device->SetTexture(0, depth);
    const float quad[4][4] = {{-0.5f, -0.5f, 0, 1}, {7.5f, -0.5f, 0, 1},
                             {-0.5f, 7.5f, 0, 1}, {7.5f, 7.5f, 0, 1}};
    for (int enabled = 0; enabled < 2; ++enabled)
    {
        for (int shadowed = 0; shadowed < 2; ++shadowed)
        {
            float constants[99][4] = {};
            constants[0][2] = constants[0][3] = 64;
            constants[1][0] = 8;
            constants[1][2] = constants[1][3] = 1.0f / 64;
            constants[2][0] = constants[2][1] = 1;
            constants[3][0] = 0.940376f;
            constants[3][1] = -0.3761504f;
            constants[3][2] = 1000;
            constants[3][3] = 0.94f;
            for (int row = 0; row < 4; ++row)
                constants[4 + row][row] = 1;
            constants[8][0] = constants[8][1] = 8;
            constants[8][2] = constants[8][3] = 0.125f;
            constants[9][2] = constants[9][3] = 1;
            constants[10][0] = 1.5f;
            constants[10][1] = 0.035f;
            constants[10][2] = static_cast<float>(enabled);
            constants[10][3] = 4;
            constants[11][1] = constants[11][3] = 1000;
            constants[11][2] = 850;
            constants[12][1] = 0.003f;
            constants[12][3] = 1;
            constants[13][0] = constants[13][1] = constants[13][2] = 0.1f;
            constants[14][0] = constants[14][1] = constants[14][2] = 0.75f;
            constants[14][3] = 1;
            constants[16][0] = constants[16][1] = constants[16][2] = 0.1f;
            constants[16][3] = 1;
            constants[17][0] = static_cast<float>(shadowed);
            constants[17][2] = 1000;
            device->SetPixelShaderConstantF(0, &constants[0][0], 99);
            device->BeginScene();
            const HRESULT draw = device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0]));
            device->EndScene();
            const float actual = ReadFogIntegrationOpacity(device, resources);
            const float source = enabled && shadowed ? 0.1f : 0.85f;
            const float expected = source * (1 - std::exp(-0.003f * 120 * std::sqrt(1 + 2 * 0.125f * 0.125f)));
            char label[160];
            std::snprintf(label, sizeof(label),
                          "blocked sun quality %d: foreground %d shafts %d shadowed layer %d (error %.3f)",
                          quality, foreground, enabled, shadowed, std::fabs(actual - expected));
            Check(SUCCEEDED(draw) && std::fabs(actual - expected) <= 2.0f / 255, label);
        }
    }
    device->SetTexture(0, resources.depth);
    depth->Release();
}

void CheckScreenSpaceShadows(IDirect3DDevice9* device, const FogIntegrationResources& resources)
{
    IDirect3DPixelShader9* shader = nullptr;
    IDirect3DTexture9* depth = nullptr;
    const bool ready = SUCCEEDED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(g_ps_shadow_probe),
                                                           &shader)) &&
                       SUCCEEDED(device->CreateTexture(64, 64, 1, 0, D3DFMT_R32F, D3DPOOL_MANAGED, &depth, nullptr));
    Check(ready, "screen-space shadow shader and depth fixture created");
    if (!ready)
    {
        if (shader)
            shader->Release();
        if (depth)
            depth->Release();
        return;
    }
    device->SetPixelShader(shader);
    device->SetTexture(0, depth);
    device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    struct ShadowCase
    {
        const char* name;
        float sampleZ;
        float lightX;
        float lightZ;
        float wallZ;
        int firstColumn;
        int endColumn;
        float expected;
        float backgroundZ = 0;
        int backgroundEndColumn = 64;
    };
    const ShadowCase cases[] = {
        {"clear sky remains lit", 4, 0, 1, 0, 0, 64, 1},
        {"rock at 40 yards blocks forward scattering", 4, 0, 1, 40, 0, 64, 0},
        {"rock at 120 yards blocks forward scattering", 4, 0, 1, 120, 0, 64, 0},
        {"rock at 800 yards blocks forward scattering", 4, 0, 1, 800, 0, 64, 0},
        {"geometry behind the light ray leaves it lit", 120, 0, 1, 40, 0, 64, 1},
        {"light facing the camera finds a nearer blocker", 120, 0, -1, 40, 0, 64, 0},
        {"offset rock casts into foreground fog", 10, 0.6f, 0.8f, 100, 50, 61, 0},
        {"blocker outside the light ray leaves sky lit", 10, 0.6f, 0.8f, 100, 0, 20, 1},
        {"ray parallel to the image finds side geometry", 80, 1, 0, 70, 46, 58, 0},
        {"parallel ray through clear sky stays lit", 80, 1, 0, 0, 0, 64, 1},
        {"foreground silhouette does not shadow distant fog", 800, 0.6f, 0.8f, 40, 0, 64, 1},
        {"foreground silhouette cannot uncover a hidden hillside", 20, 0.6f, 0.8f, 2, 42, 64, 0, 120},
        {"foreground silhouette against open sky stays finite", 20, 0.6f, 0.8f, 2, 42, 64, 1},
        {"occlusion behind a silhouette covers the entire light ray", 800, 0.6f, 0.8f, 2, 42, 64, 0, 900, 34},
    };
    const float quad[4][4] = {{-0.5f, -0.5f, 0, 1}, {7.5f, -0.5f, 0, 1},
                             {-0.5f, 7.5f, 0, 1}, {7.5f, 7.5f, 0, 1}};
    for (const ShadowCase& sample : cases)
    {
        D3DLOCKED_RECT locked = {};
        bool passed = SUCCEEDED(depth->LockRect(0, &locked, nullptr, 0));
        if (passed)
        {
            for (int y = 0; y < 64; ++y)
            {
                float* row = reinterpret_cast<float*>(static_cast<unsigned char*>(locked.pBits) + y * locked.Pitch);
                for (int x = 0; x < 64; ++x)
                {
                    const float z = x >= sample.firstColumn && x < sample.endColumn
                                        ? sample.wallZ : (x < sample.backgroundEndColumn ? sample.backgroundZ : 0);
                    row[x] = z > 0 ? 1.0004f - 0.40016f / z : 1.0f;
                }
            }
            depth->UnlockRect(0);
        }
        float worst = 0;
        for (int phase = 0; phase < 4 && passed; ++phase)
        {
            float constants[80][4] = {};
            constants[0][2] = constants[0][3] = 64;
            constants[1][2] = constants[1][3] = 1.0f / 64;
            constants[2][0] = constants[2][1] = 1;
            constants[3][0] = 1.0004f;
            constants[3][1] = -0.40016f;
            constants[3][2] = 1000;
            constants[3][3] = 1;
            constants[9][0] = sample.lightX;
            constants[9][2] = sample.lightZ;
            constants[10][0] = 1.5f;
            constants[10][1] = 0.035f;
            constants[10][2] = 1;
            constants[10][3] = 4;
            constants[11][3] = 1000;
            constants[79][2] = sample.sampleZ;
            constants[79][3] = 0.125f + phase * 0.25f;
            passed = SUCCEEDED(device->SetPixelShaderConstantF(0, &constants[0][0], 80)) && passed;
            device->BeginScene();
            passed = SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0]))) && passed;
            device->EndScene();
            const float actual = ReadFogIntegrationOpacity(device, resources);
            worst = std::max(worst, std::fabs(actual - sample.expected));
        }
        char label[160];
        std::snprintf(label, sizeof(label), "screen-space shadow: %s (error %.3f)", sample.name, worst);
        Check(passed && worst <= 1.0f / 255, label);
    }
    device->SetTexture(0, resources.depth);
    device->SetPixelShader(nullptr);
    depth->Release();
    shader->Release();
}
