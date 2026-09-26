#pragma once

namespace local_light_gpu
{
struct Light
{
    double along = 0;
    double perpendicular = 0;
    double radius = 0;
    double colour[3] = {};
    double attenuation[3] = {1, 0, 0};
};

struct Medium
{
    double start;
    double end;
    double density;
};

struct Result
{
    float colour[3] = {};
    float opacity = 0;
    bool valid = false;
};

Result Reference(const Medium& medium, const Light* lights, int count)
{
    constexpr int samples = 32768;
    const double step = (medium.end - medium.start) / samples;
    double integral[3] = {};
    for (int sample = 0; sample < samples; ++sample)
    {
        const double distance = medium.start + (sample + 0.5) * step;
        const double transmission = std::exp(-medium.density * (distance - medium.start));
        for (int light = 0; light < count; ++light)
        {
            const Light& source = lights[light];
            const double along = distance - source.along;
            const double squared = along * along + source.perpendicular * source.perpendicular;
            const double separation = std::sqrt(squared);
            if (separation >= source.radius)
                continue;
            const double attenuation = (std::max)(1.0, source.attenuation[0] +
                                                        source.attenuation[1] * separation +
                                                        source.attenuation[2] * squared);
            const double fade = (std::min)(1.0, (source.radius - separation) * 4.0 / source.radius);
            for (int channel = 0; channel < 3; ++channel)
                integral[channel] += source.colour[channel] * transmission * medium.density * step *
                                     fade / attenuation;
        }
    }
    Result result;
    for (int channel = 0; channel < 3; ++channel)
        result.colour[channel] = static_cast<float>(integral[channel]);
    result.opacity = static_cast<float>(1.0 - std::exp(-medium.density * (medium.end - medium.start)));
    result.valid = true;
    return result;
}

bool FillLights(IDirect3DTexture9* texture, const Light* lights, int count)
{
    D3DLOCKED_RECT locked = {};
    if (FAILED(texture->LockRect(0, &locked, nullptr, 0)))
        return false;
    auto* values = static_cast<float*>(locked.pBits);
    std::memset(values, 0, 32 * 4 * sizeof(float));
    const Vec3 direction = Norm({0.125f, -0.125f, 1.0f});
    const Vec3 perpendicular = Norm({1, 1, 0});
    for (int i = 0; i < count; ++i)
    {
        const Light& light = lights[i];
        values[i * 12] = static_cast<float>(direction.x * light.along + perpendicular.x * light.perpendicular);
        values[i * 12 + 1] = static_cast<float>(direction.y * light.along + perpendicular.y * light.perpendicular);
        values[i * 12 + 2] = static_cast<float>(direction.z * light.along);
        values[i * 12 + 3] = static_cast<float>(light.radius);
        for (int channel = 0; channel < 3; ++channel)
        {
            values[i * 12 + 4 + channel] = static_cast<float>(light.colour[channel]);
            values[i * 12 + 8 + channel] = static_cast<float>(light.attenuation[channel]);
        }
    }
    return SUCCEEDED(texture->UnlockRect(0));
}

float DecodeHalf(unsigned short value)
{
    const int exponent = (value >> 10) & 31;
    if (exponent == 31)
        return std::nanf("");
    const float mantissa = static_cast<float>(value & 1023) + (exponent > 0 ? 1024.0f : 0.0f);
    return std::ldexp(mantissa, exponent > 0 ? exponent - 25 : -24) * ((value & 32768) ? -1.0f : 1.0f);
}

Result Draw(IDirect3DDevice9* device, const FogIntegrationResources& resources, IDirect3DTexture9* lightTexture,
             const Medium& medium, const Light* lights, int count, D3DFORMAT format = D3DFMT_A8R8G8B8,
             int layerCount = 1, float maxDistance = 1000.0f, float marchLightLimit = 0.0f)
{
    Result result;
    if (!FillLights(lightTexture, lights, count))
        return result;
    device->SetTexture(8, lightTexture);
    float constants[99][4] = {};
    constants[0][2] = constants[0][3] = 8;
    constants[1][0] = constants[1][2] = constants[1][3] = 1;
    constants[2][0] = constants[2][1] = 1;
    constants[3][0] = 1.0004f;
    constants[3][1] = -0.40016f;
    constants[3][2] = maxDistance;
    constants[3][3] = 0.94f;
    for (int row = 0; row < 4; ++row)
        constants[4 + row][row] = 1;
    constants[8][0] = constants[8][1] = 8;
    constants[8][2] = constants[8][3] = 0.125f;
    constants[9][2] = constants[9][3] = 1;
    constants[11][1] = constants[11][3] = maxDistance;
    constants[11][2] = maxDistance * 0.85f;
    for (int layer = 0; layer < layerCount; ++layer)
    {
        const int first = 12 + layer * 6;
        constants[first][0] = static_cast<float>(medium.start);
        constants[first][1] = static_cast<float>(medium.density / layerCount);
        constants[first][3] = 1;
        constants[first + 2][3] = 1;
        constants[first + 4][3] = 1;
        constants[first + 5][2] = static_cast<float>(medium.end);
    }
    constants[53][0] = static_cast<float>(count);
    constants[53][1] = marchLightLimit;
    if (FAILED(device->SetPixelShaderConstantF(0, &constants[0][0], 99)))
        return result;
    const float quad[4][4] = {{-0.5f, -0.5f, 0, 1}, {7.5f, -0.5f, 0, 1},
                             {-0.5f, 7.5f, 0, 1}, {7.5f, 7.5f, 0, 1}};
    const HRESULT begin = device->BeginScene();
    const HRESULT draw = SUCCEEDED(begin)
                             ? device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0])) : begin;
    if (SUCCEEDED(begin))
        device->EndScene();
    if (FAILED(draw) || FAILED(device->GetRenderTargetData(resources.target, resources.readback)))
        return result;
    D3DLOCKED_RECT locked = {};
    if (FAILED(resources.readback->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
        return result;
    const BYTE* row = static_cast<const BYTE*>(locked.pBits) + 4 * locked.Pitch;
    if (format == D3DFMT_A16B16G16R16F)
    {
        const auto* pixel = reinterpret_cast<const unsigned short*>(row) + 4 * 4;
        for (int channel = 0; channel < 3; ++channel)
            result.colour[channel] = DecodeHalf(pixel[channel]);
        result.opacity = DecodeHalf(pixel[3]);
    }
    else if (format == D3DFMT_A32B32G32R32F)
    {
        const auto* pixel = reinterpret_cast<const float*>(row) + 4 * 4;
        for (int channel = 0; channel < 3; ++channel)
            result.colour[channel] = pixel[channel];
        result.opacity = pixel[3];
    }
    else
    {
        const BYTE* pixel = row + 4 * 4;
        result.colour[0] = pixel[2] / 255.0f;
        result.colour[1] = pixel[1] / 255.0f;
        result.colour[2] = pixel[0] / 255.0f;
        result.opacity = pixel[3] / 255.0f;
    }
    result.valid = SUCCEEDED(resources.readback->UnlockRect());
    return result;
}

void CheckLocalLightIntegration(IDirect3DDevice9* device)
{
    FogIntegrationResources resources;
    const bool targetsReady = CreateFogIntegrationResources(device, resources);
    Check(targetsReady, "local light GPU integration targets created");
    if (!targetsReady)
        return;
    IDirect3DTexture9* lights = nullptr;
    const bool textureReady = SUCCEEDED(device->CreateTexture(32, 1, 1, 0, D3DFMT_A32B32G32R32F,
                                                              D3DPOOL_MANAGED, &lights, nullptr));
    Check(textureReady, "local light floating-point parameter texture created");
    if (!textureReady)
        return;
    const D3DVIEWPORT9 viewport = {0, 0, 8, 8, 0, 1};
    device->SetDepthStencilSurface(nullptr);
    device->SetRenderTarget(0, resources.target);
    device->SetViewport(&viewport);
    device->SetVertexShader(nullptr);
    device->SetFVF(D3DFVF_XYZRHW);
    device->SetTexture(0, resources.depth);
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
    struct Scenario
    {
        const char* name;
        Medium medium;
        Light lights[8];
        int count;
        bool narrowVolume;
    };
    const Light uniform = {150, 0, 1000, {0.3, 0.7, 0.1}, {1, 0, 0}};
    Scenario scenarios[] = {
        {"disabled lights preserve black radiance", {100, 200, 0.01}, {uniform}, 0, false},
        {"zero RGB contributes no radiance", {100, 200, 0.01}, {{150, 0, 1000, {0, 0, 0}}}, 1, false},
        {"isolated RGB light follows analytic transmission", {100, 200, 0.01}, {uniform}, 1, false},
        {"attenuation denominator has a floor of one", {100, 200, 0.01},
            {{150, 0, 1000, {0.3, 0.7, 0.1}, {0.1, 0, 0}}}, 1, false},
        {"distance attenuation matches the independent integral", {100, 200, 0.01},
            {{150, 100, 400, {0.3, 0.7, 0.1}, {1, 0.002, 0.0001}}}, 1, false},
        {"outer quarter of the light radius fades smoothly", {145, 155, 0.1},
            {{150, 80, 100, {0.3, 0.7, 0.1}}}, 1, false},
        {"light missing the view ray contributes nothing", {100, 200, 0.01},
            {{150, 200, 50, {0.3, 0.7, 0.1}}}, 1, false},
        {"light behind the camera contributes nothing", {100, 200, 0.01},
            {{-100, 0, 40, {0.3, 0.7, 0.1}}}, 1, false},
        {"light outside the fog layer contributes nothing", {100, 200, 0.01},
            {{850, 0, 10, {0.3, 0.7, 0.1}}}, 1, false},
        {"two-yard fog interval receives analytic local light", {851, 853, 0.5},
            {{852, 0, 32, {0.3, 0.7, 0.1}}}, 1, false},
        {"two-yard light volume survives a coarse march cell", {851, 853, 0.05},
            {{852, 0, 1, {0.3, 0.7, 0.1}}}, 1, true},
        {"two overlapping lights add radiance", {100, 200, 0.01},
            {{150, 0, 1000, {0.15, 0.35, 0.05}}, {150, 0, 1000, {0.15, 0.35, 0.05}}}, 2, false},
        {"eight overlapping lights add radiance", {100, 200, 0.01}, {}, 8, false},
    };
    for (Light& light : scenarios[12].lights)
        light = {150, 0, 1000, {0.0375, 0.0875, 0.0125}};
    FogIntegrationResources extended[2];
    const D3DFORMAT extendedFormats[] = {D3DFMT_A16B16G16R16F, D3DFMT_A32B32G32R32F};
    const char* extendedNames[] = {"FP16", "FP32"};
    bool extendedReady[2] = {};
    for (int format = 0; format < 2; ++format)
    {
        extendedReady[format] = SUCCEEDED(device->CreateRenderTarget(
            8, 8, extendedFormats[format], D3DMULTISAMPLE_NONE, 0, FALSE, &extended[format].target, nullptr)) &&
            SUCCEEDED(device->CreateOffscreenPlainSurface(8, 8, extendedFormats[format], D3DPOOL_SYSTEMMEM,
                                                          &extended[format].readback, nullptr));
        if (!extendedReady[format])
            std::printf("SKIP: %s local-light overflow regression requires its render target and readback\n",
                        extendedNames[format]);
    }
    const BYTE* shaders[] = {g_ps_march_low, g_ps_march_mid, g_ps_march_high};
    Result observed[13][3] = {};
    for (int quality = 0; quality < 3; ++quality)
    {
        IDirect3DPixelShader9* shader = nullptr;
        const bool shaderReady = SUCCEEDED(device->CreatePixelShader(
            reinterpret_cast<const DWORD*>(shaders[quality]), &shader));
        Check(shaderReady, "local light integration march shader created");
        if (!shaderReady)
            continue;
        device->SetPixelShader(shader);
        for (int scene = 0; scene < 13; ++scene)
        {
            const Scenario& scenario = scenarios[scene];
            const Result expected = Reference(scenario.medium, scenario.lights, scenario.count);
            const Result actual = Draw(device, resources, lights, scenario.medium, scenario.lights, scenario.count);
            observed[scene][quality] = actual;
            bool passed = actual.valid && std::fabs(actual.opacity - expected.opacity) <= 1.0f / 255.0f;
            if (scenario.narrowVolume)
                passed = passed && actual.colour[1] > 0.04f && actual.colour[1] < 0.09f;
            else
                for (int channel = 0; channel < 3; ++channel)
                    passed = passed && std::fabs(actual.colour[channel] - expected.colour[channel]) <= 2.0f / 255.0f;
            char label[192];
            std::snprintf(label, sizeof(label), "local light quality %d: %s, opacity preserved", quality + 1,
                          scenario.name);
            Check(passed, label);
        }
        Light stressLights[8];
        for (Light& light : stressLights)
            light = {100, 0, 200, {0.0375, 0.0875, 0.0125}};
        const Medium stressMedium = {0, 200, 0.0025};
        const Result stress = Draw(device, resources, lights, stressMedium, stressLights, 8,
                                   D3DFMT_A8R8G8B8, 4, 200);
        const float stressOpacity = 1.0f - std::exp(-0.5f);
        const float stressColour[3] = {0.3f, 0.7f, 0.1f};
        bool stressPassed = stress.valid && std::fabs(stress.opacity - stressOpacity) <= 1.0f / 255.0f;
        for (int channel = 0; channel < 3; ++channel)
            stressPassed = stressPassed && std::fabs(stress.colour[channel] -
                stressColour[channel] * stressOpacity) <= 2.0f / 255.0f;
        char stressLabel[128];
        std::snprintf(stressLabel, sizeof(stressLabel),
                      "local light quality %d: full-length four-layer eight-light loops remain analytic", quality + 1);
        Check(stressPassed, stressLabel);
        const Medium boundedMedia[] = {{851, 853, 0.05}, {901.01, 920, 0.01}, {100, 150, 0.01}};
        const Light boundedLights[] = {
            {852, -0.25, 1, {0.3, 0.7, 0.1}},
            {900, 0, 1, {0.3, 0.7, 0.1}},
            {-10, -10, 200, {0.3, 0.7, 0.1}},
        };
        const char* boundNames[] = {
            "thin distant light survives conservative range bound",
            "fog beyond light range keeps its original opacity",
            "negative light coordinates preserve overlapping illumination",
        };
        for (int scene = 0; scene < 3; ++scene)
        {
            const Light& light = boundedLights[scene];
            const float limit = static_cast<float>(std::hypot(light.along, light.perpendicular) +
                                                   light.radius + 0.001);
            const Result unbounded = Draw(device, resources, lights, boundedMedia[scene], &light, 1);
            const Result bounded = Draw(device, resources, lights, boundedMedia[scene], &light, 1,
                                        D3DFMT_A8R8G8B8, 1, 1000, limit);
            bool boundPassed = unbounded.valid && bounded.valid &&
                               std::fabs(unbounded.opacity - bounded.opacity) <= 1.0f / 255.0f;
            for (int channel = 0; channel < 3; ++channel)
                boundPassed = boundPassed && std::fabs(unbounded.colour[channel] - bounded.colour[channel]) <=
                                                 1.0f / 255.0f;
            boundPassed = boundPassed && (scene == 1 ? bounded.colour[1] == 0.0f : bounded.colour[1] > 0.02f);
            char label[160];
            std::snprintf(label, sizeof(label), "local light quality %d: %s", quality + 1, boundNames[scene]);
            Check(boundPassed, label);
        }
        for (int format = 0; format < 2; ++format)
        {
            if (!extendedReady[format])
                continue;
            device->SetRenderTarget(0, extended[format].target);
            const Light intense = {150, 0, 1000, {5.0e9, 1.0e9, 1.0e8}};
            const Medium medium = {100, 200, 0.01};
            const Result actual = Draw(device, extended[format], lights, medium, &intense, 1,
                                       extendedFormats[format]);
            bool bounded = actual.valid && std::isfinite(actual.opacity) &&
                           std::fabs(actual.opacity - (1.0f - std::exp(-1.0f))) < 0.001f;
            for (float channel : actual.colour)
                bounded = bounded && std::isfinite(channel) && channel == 65504.0f;
            char label[128];
            std::snprintf(label, sizeof(label), "local light quality %d: intense RGB obeys FP16 bounds in %s storage",
                          quality + 1, extendedNames[format]);
            Check(bounded, label);
            const Light faint = {50, 0, 1000, {1.0e7, 2.0e7, 0.5e7}};
            const Medium faintMedium = {0, 100, 1.0e-10};
            const Result faintExpected = Reference(faintMedium, &faint, 1);
            const Result faintActual = Draw(device, extended[format], lights, faintMedium, &faint, 1,
                                            extendedFormats[format]);
            bool faintPassed = faintActual.valid && std::isfinite(faintActual.opacity) &&
                               std::fabs(faintActual.opacity - faintExpected.opacity) < 1.0e-7f;
            for (int channel = 0; channel < 3; ++channel)
                faintPassed = faintPassed && std::isfinite(faintActual.colour[channel]) &&
                              std::fabs(faintActual.colour[channel] - faintExpected.colour[channel]) < 0.0005f;
            std::snprintf(label, sizeof(label), "local light quality %d: thin medium preserves HDR radiance in %s",
                          quality + 1, extendedNames[format]);
            Check(faintPassed, label);
            device->SetRenderTarget(0, resources.target);
        }
        shader->Release();
    }
    for (int scene = 0; scene < 13; ++scene)
    {
        bool stable = true;
        for (int quality = 1; quality < 3; ++quality)
            for (int channel = 0; channel < 3; ++channel)
                stable = stable && observed[scene][quality].valid && observed[scene][0].valid &&
                         std::fabs(observed[scene][quality].colour[channel] - observed[scene][0].colour[channel]) <=
                             2.0f / 255.0f;
        char label[192];
        std::snprintf(label, sizeof(label), "local light quality agreement: %s", scenarios[scene].name);
        Check(stable, label);
    }
    device->SetTexture(8, nullptr);
    device->SetRenderTarget(0, resources.previousTarget);
    device->SetDepthStencilSurface(resources.previousDepth);
    resources.previousState->Apply();
    lights->Release();
}
}
