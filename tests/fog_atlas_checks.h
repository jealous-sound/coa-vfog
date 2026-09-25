#pragma once

#include "ps_atlas_low.h"
#include "ps_atlas_mid.h"
#include "ps_atlas_high.h"
#include "ps_atlas_prefix.h"

namespace fog_atlas_gpu
{
struct Layout
{
    UINT tileWidth;
    UINT tileHeight;
    UINT columns;
    UINT slices;

    UINT Width() const { return tileWidth * columns; }
    UINT Height() const { return tileHeight * ((slices + columns - 1) / columns); }
};

struct Resources
{
    IDirect3DDevice9* device;
    IDirect3DTexture9* intervals = nullptr;
    IDirect3DSurface9* intervalTarget = nullptr;
    IDirect3DPixelShader9* prefix = nullptr;
    IDirect3DSurface9* target = nullptr;
    IDirect3DSurface9* readback = nullptr;
    IDirect3DTexture9* depth = nullptr;
    IDirect3DTexture9* lights = nullptr;
    IDirect3DSurface9* previousTarget = nullptr;
    IDirect3DSurface9* previousDepth = nullptr;
    IDirect3DStateBlock9* previousState = nullptr;

    explicit Resources(IDirect3DDevice9* value) : device(value) {}

    ~Resources()
    {
        if (previousState)
        {
            device->SetRenderTarget(0, previousTarget);
            device->SetDepthStencilSurface(previousDepth);
            previousState->Apply();
            previousState->Release();
        }
        if (previousTarget)
            previousTarget->Release();
        if (previousDepth)
            previousDepth->Release();
        if (target)
            target->Release();
        if (intervalTarget)
            intervalTarget->Release();
        if (intervals)
            intervals->Release();
        if (prefix)
            prefix->Release();
        if (readback)
            readback->Release();
        if (depth)
            depth->Release();
        if (lights)
            lights->Release();
    }
};

bool CreateResources(IDirect3DDevice9* device, const Layout& layout, Resources& resources)
{
    return SUCCEEDED(device->CreateTexture(layout.Width(), layout.Height(), 1, D3DUSAGE_RENDERTARGET,
                                              D3DFMT_A16B16G16R16F, D3DPOOL_DEFAULT, &resources.intervals, nullptr)) &&
           SUCCEEDED(resources.intervals->GetSurfaceLevel(0, &resources.intervalTarget)) &&
           SUCCEEDED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(g_ps_atlas_prefix),
                                                  &resources.prefix)) &&
           SUCCEEDED(device->CreateRenderTarget(layout.Width(), layout.Height(), D3DFMT_A8R8G8B8,
                                                  D3DMULTISAMPLE_NONE, 0, FALSE, &resources.target, nullptr)) &&
           SUCCEEDED(device->CreateOffscreenPlainSurface(layout.Width(), layout.Height(), D3DFMT_A8R8G8B8,
                                                            D3DPOOL_SYSTEMMEM, &resources.readback, nullptr)) &&
           SUCCEEDED(device->CreateTexture(1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                                              &resources.depth, nullptr)) &&
           SUCCEEDED(device->CreateTexture(32, 1, 1, 0, D3DFMT_A32B32G32R32F, D3DPOOL_MANAGED,
                                              &resources.lights, nullptr)) &&
           SUCCEEDED(device->GetRenderTarget(0, &resources.previousTarget)) &&
           SUCCEEDED(device->GetDepthStencilSurface(&resources.previousDepth)) &&
           SUCCEEDED(device->CreateStateBlock(D3DSBT_ALL, &resources.previousState));
}

void SetState(IDirect3DDevice9* device, const Layout& layout, const Resources& resources)
{
    const D3DVIEWPORT9 viewport = {0, 0, layout.Width(), layout.Height(), 0, 1};
    device->SetDepthStencilSurface(nullptr);
    device->SetRenderTarget(0, resources.target);
    device->SetViewport(&viewport);
    device->SetVertexShader(nullptr);
    device->SetFVF(D3DFVF_XYZRHW);
    device->SetTexture(0, resources.depth);
    device->SetTexture(8, resources.lights);
    for (DWORD stage : {0u, 8u})
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
    device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
}

void Constants(const Layout& layout, float (&values)[99][4])
{
    std::memset(values, 0, sizeof(values));
    values[0][0] = 17;
    values[0][1] = 29;
    values[0][2] = 143;
    values[0][3] = 91;
    values[1][0] = values[1][2] = values[1][3] = 1;
    values[2][0] = 1.3f;
    values[2][1] = 0.8f;
    values[2][2] = 0.1f;
    values[2][3] = -0.2f;
    values[3][0] = 1.0004f;
    values[3][1] = -0.40016f;
    values[3][2] = 1000;
    values[3][3] = 0.94f;
    for (int row = 0; row < 4; ++row)
        values[4 + row][row] = 1;
    values[8][0] = values[8][1] = 8;
    values[8][2] = values[8][3] = 0.125f;
    values[9][2] = values[9][3] = 1;
    values[11][1] = values[11][3] = 1000;
    values[11][2] = 850;
    values[12][1] = 0.001f;
    values[12][3] = 1;
    values[13][0] = 0.25f;
    values[13][1] = 0.5f;
    values[13][2] = 0.75f;
    values[14][3] = 1;
    values[16][3] = 1;
    values[17][2] = 1000;
    values[80][0] = static_cast<float>(layout.tileWidth);
    values[80][1] = static_cast<float>(layout.tileHeight);
    values[80][2] = static_cast<float>(layout.columns);
    values[80][3] = static_cast<float>(layout.slices);
}

bool Draw(IDirect3DDevice9* device, const Layout& layout, const Resources& resources, IDirect3DPixelShader9* interval,
          const float (&constants)[99][4], DWORD rawDepth, std::vector<DWORD>& pixels)
{
    D3DLOCKED_RECT locked = {};
    if (FAILED(resources.depth->LockRect(0, &locked, nullptr, 0)))
        return false;
    *static_cast<DWORD*>(locked.pBits) = rawDepth;
    resources.depth->UnlockRect(0);
    if (FAILED(device->SetTexture(0, resources.depth)) ||
        FAILED(device->SetRenderTarget(0, resources.intervalTarget)) ||
        FAILED(device->SetPixelShader(interval)) ||
        FAILED(device->SetPixelShaderConstantF(0, &constants[0][0], 99)))
        return false;
    const float right = layout.Width() - 0.5f;
    const float bottom = layout.Height() - 0.5f;
    const float quad[4][4] = {{-0.5f, -0.5f, 0, 1}, {right, -0.5f, 0, 1},
                             {-0.5f, bottom, 0, 1}, {right, bottom, 0, 1}};
    if (FAILED(device->BeginScene()))
        return false;
    const bool drawn = SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0]))) &&
                       SUCCEEDED(device->SetRenderTarget(0, resources.target)) &&
                       SUCCEEDED(device->SetTexture(0, resources.intervals)) &&
                       SUCCEEDED(device->SetPixelShader(resources.prefix)) &&
                       SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0])));
    device->EndScene();
    if (!drawn || FAILED(device->GetRenderTargetData(resources.target, resources.readback)) ||
        FAILED(resources.readback->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
        return false;
    pixels.resize(layout.Width() * layout.Height());
    for (UINT y = 0; y < layout.Height(); ++y)
        std::memcpy(pixels.data() + y * layout.Width(),
                    static_cast<const BYTE*>(locked.pBits) + y * locked.Pitch, layout.Width() * sizeof(DWORD));
    return SUCCEEDED(resources.readback->UnlockRect());
}

float Channel(DWORD pixel, int channel)
{
    const int shift[] = {16, 8, 0, 24};
    return ((pixel >> shift[channel]) & 255) / 255.0f;
}

struct Layer
{
    double start;
    double limit;
    double density;
    double colour[3];
};

void SetLayers(float (&constants)[99][4], const Layer* layers, int count)
{
    std::memset(constants[12], 0, 24 * sizeof(constants[0]));
    for (int index = 0; index < count; ++index)
    {
        const Layer& layer = layers[index];
        float (*values)[4] = constants + 12 + index * 6;
        values[0][0] = static_cast<float>(layer.start);
        values[0][1] = static_cast<float>(layer.density);
        values[0][3] = 1;
        for (int channel = 0; channel < 3; ++channel)
            values[1][channel] = static_cast<float>(layer.colour[channel]);
        values[2][3] = 1;
        values[4][3] = 1;
        values[5][2] = static_cast<float>(layer.limit);
    }
}

double RayLength(const Layout& layout, const float (&constants)[99][4], UINT x, UINT y, UINT slice)
{
    const double u = static_cast<double>(x % layout.tileWidth) / (layout.tileWidth - 1);
    const double v = static_cast<double>(y % layout.tileHeight) / (layout.tileHeight - 1);
    const double rayX = (2 * u - 1 - constants[2][2]) / constants[2][0];
    const double rayY = (1 - 2 * v - constants[2][3]) / constants[2][1];
    const double fraction = static_cast<double>(slice) / (layout.slices - 1);
    return (std::min)(static_cast<double>(constants[3][2]),
                      constants[3][2] * fraction * fraction * std::sqrt(1 + rayX * rayX + rayY * rayY));
}

void Reference(double distance, const Layer* layers, int count, double (&colour)[4])
{
    std::vector<double> boundaries = {0, distance};
    for (int index = 0; index < count; ++index)
    {
        boundaries.push_back((std::min)(distance, (std::max)(0.0, layers[index].start)));
        boundaries.push_back((std::min)(distance, (std::max)(0.0, layers[index].limit)));
    }
    std::sort(boundaries.begin(), boundaries.end());
    double transmittance = 1;
    for (size_t edge = 1; edge < boundaries.size(); ++edge)
    {
        const double begin = boundaries[edge - 1];
        const double end = boundaries[edge];
        const double midpoint = (begin + end) * 0.5;
        double density = 0;
        double radiance[3] = {};
        for (int index = 0; index < count; ++index)
        {
            const Layer& layer = layers[index];
            if (midpoint <= layer.start || midpoint >= layer.limit)
                continue;
            density += layer.density;
            for (int channel = 0; channel < 3; ++channel)
                radiance[channel] += layer.density * layer.colour[channel];
        }
        if (density <= 0)
            continue;
        const double segmentTransmittance = std::exp(-density * (end - begin));
        for (int channel = 0; channel < 3; ++channel)
            colour[channel] += transmittance * radiance[channel] * (1 - segmentTransmittance) / density;
        transmittance *= segmentTransmittance;
    }
    colour[3] = 1 - transmittance;
}

float AnalyticError(const Layout& layout, const float (&constants)[99][4], const std::vector<DWORD>& pixels,
                    const Layer* layers, int count)
{
    float worst = 0;
    for (UINT y = 0; y < layout.Height(); ++y)
    {
        for (UINT x = 0; x < layout.Width(); ++x)
        {
            const UINT slice = (y / layout.tileHeight) * layout.columns + x / layout.tileWidth;
            double expected[4] = {};
            if (slice < layout.slices)
                Reference(RayLength(layout, constants, x, y, slice), layers, count, expected);
            const DWORD pixel = pixels[y * layout.Width() + x];
            for (int channel = 0; channel < 4; ++channel)
                worst = (std::max)(worst, static_cast<float>(std::fabs(Channel(pixel, channel) - expected[channel])));
        }
    }
    return worst;
}

void CheckLayerIntervals(IDirect3DDevice9* device, const Layout& layout, const Resources& resources,
                         IDirect3DPixelShader9* shader, int quality)
{
    const Layer layers[][4] = {
        {{0, 1000, 0.0002, {0.8, 0.1, 0.4}}, {0, 1000, 0.0003, {0.1, 0.6, 0.2}},
         {0, 1000, 0.0004, {0.3, 0.1, 0.7}}, {0, 1000, 0.0005, {0.6, 0.4, 0.1}}},
        {{19.4, 19.65, 1.6, {0.25, 0.5, 0.75}}, {120.05, 120.3, 0.8, {0.25, 0.5, 0.75}},
         {500.1, 500.22, 2.0, {0.25, 0.5, 0.75}}, {995.01, 995.36, 0.5, {0.25, 0.5, 0.75}}}
    };
    const char* names[] = {"four active layers", "thin clipped layers"};
    for (int scenario = 0; scenario < 2; ++scenario)
    {
        float constants[99][4];
        Constants(layout, constants);
        SetLayers(constants, layers[scenario], 4);
        std::vector<DWORD> pixels;
        const bool drawn = Draw(device, layout, resources, shader, constants, 0xFFFFFFFF, pixels);
        const float worst = drawn ? AnalyticError(layout, constants, pixels, layers[scenario], 4) : 1;
        char label[192];
        std::snprintf(label, sizeof(label),
                      "fog atlas %u slices quality %d preserves %s across intervals (%.2f/255)",
                      layout.slices, quality + 1, names[scenario], worst * 255);
        Check(drawn && worst <= 2.0f / 255.0f, label);
    }
}

void CheckOrderedColour(IDirect3DDevice9* device, const Layout& layout, const Resources& resources,
                        IDirect3DPixelShader9* shader, int quality)
{
    float constants[99][4];
    Constants(layout, constants);
    const UINT localX = layout.tileWidth / 2;
    const UINT localY = layout.tileHeight / 2;
    constants[2][2] = 2.0f * localX / (layout.tileWidth - 1) - 1;
    constants[2][3] = 1 - 2.0f * localY / (layout.tileHeight - 1);
    const double intervalScale = 1000.0 / ((layout.slices - 1) * (layout.slices - 1));
    const double opacity[] = {0.45, 0.55, 0.6, 0.2};
    Layer layers[] = {
        {intervalScale, 4 * intervalScale, 0, {0.8, 0.1, 0.05}},
        {4 * intervalScale, 9 * intervalScale, 0, {0.05, 0.9, 0.1}},
        {16 * intervalScale, 25 * intervalScale, 0, {0.05, 0.1, 0.9}},
        {25 * intervalScale, 36 * intervalScale, 0, {0.9, 0.7, 0.05}}
    };
    for (int index = 0; index < 4; ++index)
        layers[index].density = -std::log(1 - opacity[index]) / (layers[index].limit - layers[index].start);
    SetLayers(constants, layers, 4);
    std::vector<DWORD> pixels;
    const bool drawn = Draw(device, layout, resources, shader, constants, 0xFFFFFFFF, pixels);
    float worst = 0;
    for (UINT slice = 0; drawn && slice < layout.slices; ++slice)
    {
        const UINT x = (slice % layout.columns) * layout.tileWidth + localX;
        const UINT y = (slice / layout.columns) * layout.tileHeight + localY;
        double expected[4] = {};
        Reference(RayLength(layout, constants, x, y, slice), layers, 4, expected);
        const DWORD pixel = pixels[y * layout.Width() + x];
        for (int channel = 0; channel < 4; ++channel)
            worst = (std::max)(worst, static_cast<float>(std::fabs(Channel(pixel, channel) - expected[channel])));
    }
    char label[192];
    std::snprintf(label, sizeof(label),
                  "fog atlas %u slices quality %d attenuates later colours through earlier intervals (%.2f/255)",
                  layout.slices, quality + 1, worst * 255);
    Check(drawn && worst <= 2.0f / 255.0f, label);
}

void CheckFogAtlas(IDirect3DDevice9* device)
{
    const Layout layouts[] = {{5, 3, 3, 10}, {4, 4, 4, 32}};
    const BYTE* shaders[] = {g_ps_atlas_low, g_ps_atlas_mid, g_ps_atlas_high};
    for (const Layout& layout : layouts)
    {
        Resources resources(device);
        const bool ready = CreateResources(device, layout, resources);
        Check(ready, "fog atlas test targets and parameter textures created");
        if (!ready)
            continue;
        SetState(device, layout, resources);
        for (int quality = 0; quality < 3; ++quality)
        {
            IDirect3DPixelShader9* shader = nullptr;
            const bool shaderReady = SUCCEEDED(device->CreatePixelShader(
                reinterpret_cast<const DWORD*>(shaders[quality]), &shader));
            Check(shaderReady, "fog atlas shader created");
            if (!shaderReady)
                continue;
            float constants[99][4];
            Constants(layout, constants);
            std::vector<DWORD> sky;
            std::vector<DWORD> nearSurface;
            bool analytic = Draw(device, layout, resources, shader, constants, 0xFFFFFFFF, sky);
            const bool nearReady = Draw(device, layout, resources, shader, constants, 0x00000000, nearSurface);
            Check(analytic && nearReady && sky == nearSurface,
                  "fog atlas slice depth is independent of opaque foreground and sky depth");
            const Layer homogeneous = {0, 1000, 0.001, {0.25, 0.5, 0.75}};
            const float worst = analytic ? AnalyticError(layout, constants, sky, &homogeneous, 1) : 1;
            char label[192];
            std::snprintf(label, sizeof(label),
                          "fog atlas %u slices quality %d matches analytic rays at every tile pixel (%.2f/255)",
                          layout.slices, quality + 1, worst * 255);
            Check(analytic && worst <= 2.0f / 255.0f, label);
            CheckLayerIntervals(device, layout, resources, shader, quality);
            CheckOrderedColour(device, layout, resources, shader, quality);

            if (layout.tileWidth == 5)
            {
                constants[2][0] = constants[2][1] = 1;
                constants[2][2] = -0.125f;
                constants[2][3] = 0.125f;
                constants[12][0] = 100;
                constants[12][1] = 0.01f;
                constants[13][0] = constants[13][1] = constants[13][2] = 0;
                constants[17][2] = 200;
                constants[53][0] = 1;
                const local_light_gpu::Light light = {150, 0, 1000, {0.3, 0.7, 0.1}, {1, 0, 0}};
                const local_light_gpu::Medium medium = {100, 200, 0.01};
                const local_light_gpu::Result expected = local_light_gpu::Reference(medium, &light, 1);
                bool lit = local_light_gpu::FillLights(resources.lights, &light, 1) &&
                           Draw(device, layout, resources, shader, constants, 0xFFFFFFFF, sky);
                if (lit)
                {
                    const UINT lastSlice = layout.slices - 1;
                    const UINT x = (lastSlice % layout.columns) * layout.tileWidth + layout.tileWidth / 2;
                    const UINT y = (lastSlice / layout.columns) * layout.tileHeight + layout.tileHeight / 2;
                    const DWORD pixel = sky[y * layout.Width() + x];
                    for (int channel = 0; channel < 3; ++channel)
                        lit = lit && std::fabs(Channel(pixel, channel) - expected.colour[channel]) <= 2.0f / 255.0f;
                    lit = lit && std::fabs(Channel(pixel, 3) - expected.opacity) <= 1.0f / 255.0f;
                }
                Check(lit, "fog atlas receives the same coloured point-light input as the opaque march");
            }
            shader->Release();
        }
    }
}
}
