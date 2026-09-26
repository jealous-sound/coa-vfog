#pragma once

bool SupportsHardwareShadowTexture(IDirect3DDevice9* device)
{
    IDirect3D9* d3d = nullptr;
    D3DDEVICE_CREATION_PARAMETERS creation = {};
    D3DDISPLAYMODE display = {};
    if (FAILED(device->GetCreationParameters(&creation)) || FAILED(device->GetDisplayMode(0, &display)) ||
        FAILED(device->GetDirect3D(&d3d)) || !d3d)
        return false;
    const HRESULT supported = d3d->CheckDeviceFormat(creation.AdapterOrdinal, creation.DeviceType, display.Format,
                                                    D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, D3DFMT_D24X8);
    d3d->Release();
    return SUCCEEDED(supported);
}

void CheckHardwareWorldShadowIntegration(IDirect3DDevice9* device)
{
    if (!SupportsHardwareShadowTexture(device))
    {
        std::printf("SKIP native D24X8 world shadow comparisons: depth textures unsupported by this device\n");
        return;
    }
    FogIntegrationResources resources;
    const bool targetsReady = CreateFogIntegrationResources(device, resources);
    Check(targetsReady, "hardware world shadow integration render targets created");
    if (!targetsReady)
        return;
    IDirect3DTexture9* shadow = nullptr;
    IDirect3DSurface9* shadowDepth = nullptr;
    bool ready = SUCCEEDED(device->CreateTexture(8, 8, 1, D3DUSAGE_DEPTHSTENCIL, D3DFMT_D24X8,
                                                 D3DPOOL_DEFAULT, &shadow, nullptr)) &&
                 SUCCEEDED(shadow->GetSurfaceLevel(0, &shadowDepth));
    if (ready)
    {
        const D3DVIEWPORT9 viewport = {0, 0, 8, 8, 0, 1};
        const D3DRECT rightHalf = {4, 0, 8, 8};
        ready = SUCCEEDED(device->SetDepthStencilSurface(nullptr)) &&
                SUCCEEDED(device->SetRenderTarget(0, resources.target)) &&
                SUCCEEDED(device->SetViewport(&viewport)) &&
                SUCCEEDED(device->SetDepthStencilSurface(shadowDepth)) &&
                SUCCEEDED(device->Clear(0, nullptr, D3DCLEAR_ZBUFFER, 0, 0.25f, 0)) &&
                SUCCEEDED(device->Clear(1, &rightHalf, D3DCLEAR_ZBUFFER, 0, 0.75f, 0)) &&
                SUCCEEDED(device->SetDepthStencilSurface(nullptr));
    }
    Check(ready, "native D24X8 shadow fixture stores two caster depths");
    if (ready)
    {
        device->SetVertexShader(nullptr);
        device->SetFVF(D3DFVF_XYZRHW);
        device->SetTexture(0, resources.depth);
        device->SetTexture(4, shadow);
        for (DWORD stage : {0u, 4u})
        {
            const DWORD filter = stage == 4 ? D3DTEXF_LINEAR : D3DTEXF_POINT;
            device->SetSamplerState(stage, D3DSAMP_MINFILTER, filter);
            device->SetSamplerState(stage, D3DSAMP_MAGFILTER, filter);
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
        const float quad[4][4] = {{-0.5f, -0.5f, 0, 1}, {7.5f, -0.5f, 0, 1},
                                 {-0.5f, 7.5f, 0, 1}, {7.5f, 7.5f, 0, 1}};
        struct ShadowCase
        {
            const char* name;
            float x;
            float depth;
            float visibility;
        };
        const ShadowCase cases[] = {
            {"receiver behind caster is shadowed", -0.5f, 0.5f, 0},
            {"receiver before caster is lit", -0.5f, 0.1f, 1},
            {"receiver before the farther caster is lit", 0.5f, 0.5f, 1},
            {"hardware PCF filters the caster boundary", 0, 0.5f, 0.5f},
            {"world-map coverage fades at its border", -0.845f, 0.5f, 0.5f},
        };
        const BYTE* shaders[] = {g_ps_march_low, g_ps_march_mid, g_ps_march_high};
        for (int quality = 0; quality < 3; ++quality)
        {
            IDirect3DPixelShader9* shader = nullptr;
            const bool shaderReady = SUCCEEDED(device->CreatePixelShader(
                reinterpret_cast<const DWORD*>(shaders[quality]), &shader));
            Check(shaderReady, "hardware world shadow march shader created");
            if (!shaderReady)
                continue;
            device->SetPixelShader(shader);
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
                constants[36][0] = constants[36][1] = 1;
                constants[37][3] = sample.x;
                constants[39][3] = sample.depth;
                constants[49][0] = constants[49][1] = 0.125f;
                device->SetPixelShaderConstantF(0, &constants[0][0], 99);
                const HRESULT begin = device->BeginScene();
                const HRESULT draw = SUCCEEDED(begin)
                                         ? device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0]))
                                         : begin;
                if (SUCCEEDED(begin))
                    device->EndScene();
                const float actual = ReadFogIntegrationOpacity(device, resources);
                const float expected = sample.visibility * (1.0f - std::exp(-1.0f));
                char label[180];
                std::snprintf(label, sizeof(label), "native D24X8 quality %d: %s", quality + 1, sample.name);
                Check(SUCCEEDED(draw) && std::fabs(actual - expected) <= 2.0f / 255.0f, label);
            }
            shader->Release();
        }
    }
    device->SetTexture(4, nullptr);
    device->SetDepthStencilSurface(nullptr);
    device->SetRenderTarget(0, resources.previousTarget);
    device->SetDepthStencilSurface(resources.previousDepth);
    resources.previousState->Apply();
    if (shadowDepth)
        shadowDepth->Release();
    if (shadow)
        shadow->Release();
}
