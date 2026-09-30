#pragma once

namespace classic_phase
{
constexpr float kIsotropicScale = 0.0796f;
constexpr float kForwardScale = 15.12f;
constexpr float kForwardG = 0.9f;
constexpr float kCappedG = 0.95f;
constexpr float kBeyondCapG = 0.99f;
constexpr float kSlabAuthoredDensity = 100.0f;
constexpr float kSlabDiffuse = 1.0f;
constexpr float kSlabEmissive = 0.3f;
constexpr float kSlabIntensity = 0.6f;
constexpr float kSlabTolerance = 2.0f / 255.0f;
constexpr uint32_t kSelectedLayerFlag = 0x8;
constexpr float kSunAboveViewCentreDegrees = 20.0f;
constexpr float kHaloDegreesBelowSun = 6.0f;
constexpr UINT kHaloViewWidth = 1280;
constexpr UINT kHaloViewHeight = 688;
constexpr int kHaloSampleRadius = 2;
constexpr float kMinHaloChannelStep = 8.0f;

void CheckPhaseScale()
{
    const float isotropic = EnergyNormalisedPhaseScale(0.0f);
    const float forward = EnergyNormalisedPhaseScale(kForwardG);
    std::printf("     energy-normalised phase scale: k(0) %.4f, k(0.9) %.2f, k(0.95) %.1f, k(0.99) %.1f\n", isotropic,
                forward, EnergyNormalisedPhaseScale(kCappedG), EnergyNormalisedPhaseScale(kBeyondCapG));
    Check(std::fabs(isotropic - kIsotropicScale) < 1e-4f && std::fabs(forward - kForwardScale) < 0.01f &&
              EnergyNormalisedPhaseScale(kBeyondCapG) == EnergyNormalisedPhaseScale(kCappedG),
          "the energy-normalised phase scale is (1+g)/(4pi(1-g)^2), capped at g 0.95");
}

AuthoredFog SlabFog(float g)
{
    AuthoredFog fog = {};
    fog.layerCount = 1;
    fog.coverage = 1.0f;
    AuthoredLayer& slab = fog.layers[0];
    slab.density = kSlabAuthoredDensity;
    slab.g = g;
    slab.intensity = kSlabIntensity;
    slab.exponent = 1.0f;
    slab.shadowMultiplier = 1.0f;
    slab.flags = kSelectedLayerFlag;
    for (int c = 0; c < 3; ++c)
    {
        slab.diffuse[c] = kSlabDiffuse;
        slab.emissive[c] = kSlabEmissive;
        slab.shadowEmissive[c] = kSlabEmissive;
    }
    return fog;
}

FrameInputs SlabFrame()
{
    const Vec3 eye = Add(kGameLikeWorldOffset, {0, 0, 9});
    return ContinentFrame(kEasternKingdoms, eye, kNoon, {0.2f, 0.2f, 0.95f}, false);
}

void CheckOnlyTheScatterChanges()
{
    Config peak;
    peak.classicPhase = 0;
    Config energy;
    energy.classicPhase = 1;
    const FrameInputs frame = SlabFrame();
    bool scatterScaled = true;
    bool mediumKept = true;
    for (float g : {0.0f, kForwardG})
    {
        const AuthoredFog fog = SlabFog(g);
        const FogLayer a = BuildFogParams(frame, peak, &fog).layers[0];
        const FogLayer b = BuildFogParams(frame, energy, &fog).layers[0];
        const float scale = EnergyNormalisedPhaseScale(a.g);
        for (int c = 0; c < 3; ++c)
        {
            scatterScaled = scatterScaled && std::fabs(b.diffuse[c] - a.diffuse[c] * scale) <= 1e-5f * b.diffuse[c];
            mediumKept = mediumKept && a.emissive[c] == b.emissive[c] && a.shadowEmissive[c] == b.shadowEmissive[c];
        }
        mediumKept = mediumKept && a.density == b.density && a.g == b.g && a.start == b.start;
    }
    Check(scatterScaled && mediumKept,
          "ClassicPhase=1 scales only the Classic layers' sun scattering by the energy-normalised phase");
    const FogParams derived = BuildFogParams(frame, energy, nullptr);
    const FogParams derivedPeak = BuildFogParams(frame, peak, nullptr);
    Check(std::memcmp(derived.layers, derivedPeak.layers, sizeof(derived.layers)) == 0,
          "ClassicPhase leaves the derived layers and the distance fog alone");
}

float SaturatedSlabRadiance(IDirect3DDevice9* device, const FogIntegrationResources& resources, const FogParams& fog)
{
    float constants[99][4] = {};
    constants[0][2] = constants[0][3] = 8.0f;
    constants[1][0] = 1.0f;
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
    constants[11][1] = constants[11][3] = 1000.0f;
    constants[11][2] = 850.0f;
    std::memcpy(constants[12], &fog.layers[0], sizeof(FogLayer));
    device->SetPixelShaderConstantF(0, &constants[0][0], 99);
    const float quad[4][4] = {{-0.5f, -0.5f, 0, 1}, {7.5f, -0.5f, 0, 1}, {-0.5f, 7.5f, 0, 1}, {7.5f, 7.5f, 0, 1}};
    device->BeginScene();
    device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0]));
    device->EndScene();
    return ReadFogIntegrationOpacity(device, resources);
}

void CheckIsotropicSlabSaturation(IDirect3DDevice9* device)
{
    FogIntegrationResources resources;
    IDirect3DPixelShader9* shader = nullptr;
    const bool ready = CreateFogIntegrationResources(device, resources) &&
                       SUCCEEDED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(g_ps_march_mid), &shader));
    Check(ready, "isotropic slab march target and shader created");
    if (ready)
    {
        const D3DVIEWPORT9 viewport = {0, 0, 8, 8, 0.0f, 1.0f};
        device->SetDepthStencilSurface(nullptr);
        device->SetRenderTarget(0, resources.target);
        device->SetViewport(&viewport);
        device->SetVertexShader(nullptr);
        device->SetFVF(D3DFVF_XYZRHW);
        device->SetPixelShader(shader);
        device->SetTexture(0, resources.depth);
        device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
        const FrameInputs frame = SlabFrame();
        const AuthoredFog fog = SlabFog(0.0f);
        const float emissive = std::pow(kSlabEmissive, kDisplayGamma);
        float radiance[2] = {};
        float expected[2] = {};
        for (int phase = 0; phase < 2; ++phase)
        {
            Config config;
            config.classicPhase = phase;
            radiance[phase] = SaturatedSlabRadiance(device, resources, BuildFogParams(frame, config, &fog));
            expected[phase] = emissive + kSlabDiffuse * kSlabIntensity * (phase ? 1.0f / (4.0f * kPi) : 1.0f);
        }
        std::printf("     saturated g=0 slab: ClassicPhase 0 %.3f (expected %.3f), ClassicPhase 1 %.3f (expected "
                    "%.3f, emissive + diffuse * I / 4pi)\n",
                    radiance[0], expected[0], radiance[1], expected[1]);
        Check(std::fabs(radiance[1] - expected[1]) <= kSlabTolerance,
              "with ClassicPhase=1 a saturated g=0 slab reaches emissive + diffuse * I / (4pi)");
        Check(std::fabs(radiance[0] - expected[0]) <= kSlabTolerance,
              "with ClassicPhase=0 a saturated g=0 slab keeps emissive + diffuse * I");
    }
    device->SetRenderTarget(0, resources.previousTarget);
    device->SetDepthStencilSurface(resources.previousDepth);
    if (resources.previousState)
        resources.previousState->Apply();
    if (shader)
        shader->Release();
}

struct SunsetHalo
{
    bool rendered = false;
    const char* skip = "";
    Rgb colour = {};
};

int PixelOf(float ndc, UINT extent)
{
    return static_cast<int>((ndc * 0.5f + 0.5f) * extent);
}

SunsetHalo RenderHarbourSunsetHalo(Harness& h, Config config)
{
    config.temporal = 0.0f;
    vf_test_set_config(&config);
    const Vec3 toLight = Norm(kHarbourToLight);
    const float sunAzimuth = std::atan2(toLight.y, toLight.x) / kDegree;
    const float sunElevation = std::asin(toLight.z) / kDegree;
    const Vec3 at = Add(kHarbourEye, Dir(sunAzimuth, sunElevation - kSunAboveViewCentreDegrees));
    const D3DVIEWPORT9 world = {0, 0, kHaloViewWidth, kHaloViewHeight, 0.0f, 1.0f};
    float view[16];
    float proj[16];
    CameraRelativeLookAt(kHarbourEye, at, view);
    EngineGlDepthProjection(kHarbourLoggedP11, static_cast<float>(kHaloViewWidth) / kHaloViewHeight, kHarbourNear,
                            kHarbourFar, proj);
    const FrameInputs in = HarbourSunsetInputs(view, proj, at, world, 0.0f);
    const D3DCOLOR clientClear = h.clearColor;
    h.clearColor = kHarbourSky;
    h.BeginFrame();
    h.dev->SetViewport(&world);
    SunsetHalo halo;
    halo.rendered = vf_test_render(&in, &halo.skip) != 0;
    const Image image = Capture(h.dev);
    h.dev->EndScene();
    h.dev->Present(nullptr, nullptr, nullptr, nullptr);
    h.clearColor = clientClear;
    const Vec3 d = Dir(sunAzimuth, sunElevation - kHaloDegreesBelowSun);
    const float vx = d.x * view[0] + d.y * view[4] + d.z * view[8];
    const float vy = d.x * view[1] + d.y * view[5] + d.z * view[9];
    const float vz = d.x * view[2] + d.y * view[6] + d.z * view[10];
    const int x = PixelOf(vx / vz * proj[0] + proj[8], kHaloViewWidth);
    const int y = PixelOf(-(vy / vz * proj[5] + proj[9]), kHaloViewHeight);
    halo.colour = SampleRgb(image, x, y, kHaloSampleRadius);
    return halo;
}

bool KeepsSunsetHue(const SunsetHalo& halo)
{
    const Rgb& c = halo.colour;
    return halo.rendered && c.r >= c.g + kMinHaloChannelStep && c.g >= c.b + kMinHaloChannelStep;
}

void CheckSunsetHaloKeepsItsHue(Harness& h, const std::wstring& shippedIni)
{
    Config saved;
    vf_test_get_config(&saved);
    ConfigStore shipped;
    shipped.Load(NarrowPath(shippedIni));
    const SunsetHalo fromIni = RenderHarbourSunsetHalo(h, shipped.Get());
    const SunsetHalo builtIn = RenderHarbourSunsetHalo(h, Config());
    vf_test_set_config(&saved);
    std::printf("     harbour sunset %.0f deg below the sun: %.0f %.0f %.0f with the shipped INI (ClassicPhase %d), "
                "%.0f %.0f %.0f with the built-in defaults (ClassicPhase %d)\n",
                kHaloDegreesBelowSun, fromIni.colour.r, fromIni.colour.g, fromIni.colour.b,
                shipped.Get().classicPhase, builtIn.colour.r, builtIn.colour.g, builtIn.colour.b,
                Config().classicPhase);
    Check(KeepsSunsetHue(fromIni) && KeepsSunsetHue(builtIn),
          (std::string("the harbour's sunset halo keeps its warm hue, red over green over blue, with the shipped "
                       "settings and the built-in defaults instead of clipping to white ") +
           fromIni.skip + builtIn.skip)
              .c_str());
}

void CheckClassicPhase()
{
    CheckPhaseScale();
    CheckOnlyTheScatterChanges();
}
}
