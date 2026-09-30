#pragma once

#include "ps_client_m2_fog.h"
#include "vs_client_m2_fog.h"

namespace transparent_fog_checks
{
struct FitErrorLimits
{
    float axis;
    float side;
};

constexpr float kPushedStockFogStart = 50000.0f;
constexpr float kPushedStockFogEnd = 100000.0f;
constexpr float kAuthoredClientExponent = 0.6f;
constexpr float kNoLayerLimit = 1.0e9f;
constexpr float kFitMaxDistance = 5000.0f;
constexpr float kFitFarClip = 1000.0f;
constexpr float kFitHorizonStart = 850.0f;
constexpr int kReferenceStepsPerYard = 20;
constexpr float kFitCheckDepthStep = 1.0f;
constexpr float kFrustumEdgeNdc = 0.8f;
constexpr float kThinHomogeneousDensity = 0.002f;
constexpr FitErrorLimits kThinHomogeneousLimits = {0.025f, 0.025f};
constexpr float kGroundFogDensity = 0.006f;
constexpr float kGroundFogFalloff = 1.0f / 18.0f;
constexpr float kGroundFogBelowEye = 4.0f;
constexpr FitErrorLimits kGroundFogLimits = {0.03f, 0.06f};
constexpr FitErrorLimits kHarbourLimits = {0.03f, 0.04f};
constexpr float kEmissiveFog = 0.3f;
constexpr float kFitExposure = 1.2f;
constexpr float kFitGlow = 0.5f;
constexpr float kDisplayGammaExponent = 2.2f;
constexpr int kMaxColourLevelError = 1;
constexpr uint32_t kLightingColour = 0xFF405060u;
constexpr float kRegressionDensity = 0.004f;
constexpr float kRegressionEmissive = 0.12f;
constexpr float kEffectDepth = 10.0f;
constexpr float kEffectHalfSize = 1.0f;
constexpr float kBackgroundDepth = 800.0f;
constexpr DWORD kBackgroundColour = 0xFF303030;
constexpr int kEffectLevel = 0x66;
constexpr int kProbeRadius = 4;
constexpr float kMaxFactorError = 0.02f;
constexpr float kMaxOwnDepthLevelError = 3.0f;
constexpr int kWarmUpFrames = 2;
constexpr int kMaxEarlyDifference = 1;
constexpr int kMaxMultisampledEarlyDifference = 2;
constexpr float kRaySunRegion = 60.0f;
constexpr float kMinRayGain = 0.01f;
constexpr int kMaxRayDarkening = 1;
constexpr int kMaxLateRayDifference = 2;
constexpr float kRaysOn = 1.0f;
constexpr uint32_t kSentinelEsi = 0x51515151u;
constexpr uint32_t kSentinelEdi = 0x5D5D5D5Du;
constexpr uint32_t kSentinelEbx = 0x5B5B5B5Bu;
constexpr uint32_t kFogArgumentBytes = 16;
constexpr float kStockFogStart = 150.0f;
constexpr float kStockFogEnd = 600.0f;

struct ClientFogConstant
{
    float x, y, z, w;
};

ClientFogConstant ClientM2FogConstant(float start, float end, float exponent)
{
    return {-1.0f / (end - start), end / (end - start), exponent, 0.0f};
}

float ClientM2FogFactor(const ClientFogConstant& c, float viewDepth)
{
    return std::fmin(std::pow(std::fmax(viewDepth * c.x + c.y, 0.0f), c.z), 1.0f);
}

float ColourChannel(uint32_t colour, int shift)
{
    return static_cast<float>((colour >> shift) & 0xFF) / 255.0f;
}

FogParams EmptyFog()
{
    FogParams fog = {};
    for (FogLayer& l : fog.layers)
    {
        l.exponent = 1.0f;
        l.shadowDensity = 1.0f;
        l.endDistance = kNoLayerLimit;
    }
    fog.maxDistance = kFitMaxDistance;
    fog.farClip = kFitFarClip;
    fog.farLimit = kFitFarClip;
    fog.horizonStart = kFitHorizonStart;
    fog.lightAboveHorizon = 1.0f;
    fog.linear = true;
    return fog;
}

FogParams HomogeneousFog(float density, float emissive)
{
    FogParams fog = EmptyFog();
    FogLayer& l = fog.layers[0];
    l.density = density;
    l.isotropic = 1.0f;
    for (int c = 0; c < 3; ++c)
        l.emissive[c] = l.shadowEmissive[c] = emissive;
    return fog;
}

FogParams GroundFog(float eyeHeight)
{
    FogParams fog = HomogeneousFog(kGroundFogDensity, kEmissiveFog);
    fog.layers[0].upperHeight = eyeHeight - kGroundFogBelowEye;
    fog.layers[0].upperFalloff = kGroundFogFalloff;
    return fog;
}

struct View
{
    Vec3 eye = Add({0, 0, 9}, kGameLikeWorldOffset);
    Vec3 at = Add({100, 0, 9}, kGameLikeWorldOffset);
    D3DVIEWPORT9 world = {0, 0, 1280, 688, 0.0f, 1.0f};
    float view[16] = {};
    float proj[16] = {};
    float d3dProj[16] = {};
    FrameInputs in = {};

    View()
    {
        EngineProjection(static_cast<float>(world.Width) / world.Height, proj);
        Finish();
    }

    View(Vec3 from, Vec3 to, const D3DVIEWPORT9& viewport) : eye(from), at(to), world(viewport)
    {
        EngineProjection(static_cast<float>(world.Width) / world.Height, proj);
        Finish();
    }

    void Finish()
    {
        CameraRelativeLookAt(eye, at, view);
        RemapToD3DDepthRange(proj, d3dProj);
        in = MakeInputs(view, proj, eye, at, world);
    }

    float RawDepthAt(float viewDepth) const { return d3dProj[10] + d3dProj[14] / viewDepth; }
};

float ReferenceTransmittance(const FogParams& fog, float cameraHeight, float rise, float distance)
{
    const int steps = std::max(1, static_cast<int>(distance * kReferenceStepsPerYard));
    const float dt = distance / steps;
    double opticalDepth = 0.0;
    for (int i = 0; i < steps; ++i)
    {
        const float t = (i + 0.5f) * dt;
        const float height = cameraHeight + rise * t;
        for (const FogLayer& l : fog.layers)
        {
            if (t < l.start || t > l.endDistance || l.density <= 0.0f)
                continue;
            const float curve =
                1.0f + l.strength * std::pow(std::fmin(std::fmax(t - l.start, 0.0f) / fog.maxDistance, 1.0f) + 1e-6f,
                                             l.exponent);
            const float heightFactor = std::fmin(std::exp((l.upperHeight - height) * l.upperFalloff), 1.0f) *
                                       std::fmin(std::exp((height - l.lowerHeight) * l.lowerFalloff), 1.0f);
            const float shadow = 1.0f + (l.shadowDensity - 1.0f) * l.shadowed * (1.0f - fog.lightAboveHorizon);
            opticalDepth += l.density * curve * heightFactor * shadow * dt;
        }
    }
    return static_cast<float>(std::exp(-opticalDepth));
}

float FittedClientFactor(const StockFogFit& fit, float viewDepth)
{
    const uint32_t lighting = kLightingColour;
    M2BatchFogArgs args = {kPushedStockFogStart, kPushedStockFogEnd, kAuthoredClientExponent, &lighting};
    ApplyStockFogFit(fit, args);
    return ClientM2FogFactor(ClientM2FogConstant(args.start, args.end, args.exponent), viewDepth);
}

struct FitError
{
    float axis = 0.0f;
    float frustumEdge = 0.0f;
};

FitError MeasureFitError(const FogParams& drawn, const FrameInputs& in, const StockFogFit& fit)
{
    const FogParams fog = WithMeanNoise(drawn);
    const float* view = in.cameraRelativeView;
    const float axisRise = view[10];
    const float edgeRayX = kFrustumEdgeNdc / in.glProjection[0];
    const float edgeRay = std::sqrt(1.0f + edgeRayX * edgeRayX);
    FitError error;
    for (float z = 0.0f; z <= kStockFogFitDepth; z += kFitCheckDepthStep)
    {
        const float fitted = FittedClientFactor(fit, z);
        error.axis = std::fmax(error.axis,
                               std::fabs(fitted - ReferenceTransmittance(fog, in.camPos[2], axisRise, z)));
        error.frustumEdge = std::fmax(
            error.frustumEdge, std::fabs(fitted - ReferenceTransmittance(fog, in.camPos[2], axisRise, z * edgeRay)));
    }
    return error;
}

StockFogFit AxisOnlyFit(const FogParams& drawn, const FrameInputs& in)
{
    const FogParams fog = WithMeanNoise(drawn);
    double w = 0.0;
    double x = 0.0;
    double y = 0.0;
    double xx = 0.0;
    double xy = 0.0;
    for (float z = 0.0f; z <= kStockFogFitDepth; z += kFitCheckDepthStep)
    {
        const double t = ReferenceTransmittance(fog, in.camPos[2], in.cameraRelativeView[10], z);
        w += 1.0;
        x += z;
        y += t;
        xx += z * z;
        xy += z * t;
    }
    const double slope = (w * xy - x * y) / (w * xx - x * x);
    const double intercept = (y - slope * x) / w;
    StockFogFit fit;
    fit.fogs = true;
    fit.start = static_cast<float>((1.0 - intercept) / slope);
    fit.end = static_cast<float>(-intercept / slope);
    return fit;
}

bool CheckFitFollows(const char* name, const FogParams& fog, const FrameInputs& in, const FitErrorLimits& limits)
{
    const StockFogFit fit = FitStockFog(fog, in, {1.0f, 0.0f});
    const FitError error = MeasureFitError(fog, in, fit);
    const FitError axisOnly = MeasureFitError(fog, in, AxisOnlyFit(fog, in));
    std::printf("     %s: fitted stock fog %s start %.1f end %.1f; largest |f - T| over 0..%.0f yd %.4f on the view "
                "axis, %.4f at the side of the view (a fit along the axis alone: %.4f, %.4f)\n",
                name, fit.fogs ? "on," : "off,", fit.start, fit.end, kStockFogFitDepth, error.axis, error.frustumEdge,
                axisOnly.axis, axisOnly.frustumEdge);
    return fit.fogs && error.axis < limits.axis && error.frustumEdge < limits.side &&
           error.frustumEdge < axisOnly.frustumEdge;
}

FrameInputs HarbourLevelView(const D3DVIEWPORT9& world)
{
    float view[16];
    float proj[16];
    const Vec3 toLight = Norm(kHarbourToLight);
    const Vec3 at = Add(kHarbourEye, Norm({toLight.x, toLight.y, 0.0f}));
    CameraRelativeLookAt(kHarbourEye, at, view);
    EngineGlDepthProjection(kHarbourLoggedP11, static_cast<float>(world.Width) / world.Height, kHarbourNear,
                            kHarbourFar, proj);
    return HarbourSunsetInputs(view, proj, at, world, 0.0f);
}

int ChannelLevel(float value)
{
    return static_cast<int>(std::lround(std::fmin(std::fmax(value, 0.0f), 1.0f) * 255.0f));
}

bool ColourIsGrey(uint32_t colour, int level)
{
    for (int shift : {0, 8, 16})
        if (std::abs(static_cast<int>((colour >> shift) & 0xFF) - level) > kMaxColourLevelError)
            return false;
    return true;
}

void CheckFittedColour()
{
    const View v;
    FogParams fog = HomogeneousFog(kThinHomogeneousDensity, kEmissiveFog);
    const float exposed = kEmissiveFog * kFitExposure;
    const float displayed = std::pow(exposed, 1.0f / kDisplayGammaExponent);
    const float glowCompensated = 2.0f * displayed / (1.0f + std::sqrt(1.0f + 4.0f * kFitGlow * displayed));
    const StockFogFit linear = FitStockFog(fog, v.in, {kFitExposure, 0.0f});
    const StockFogFit glow = FitStockFog(fog, v.in, {kFitExposure, kFitGlow});
    fog.linear = false;
    const StockFogFit gamma = FitStockFog(fog, v.in, {kFitExposure, kFitGlow});
    std::printf("     fitted fog colours %08X (linear), %08X (linear, glow %.1f), %08X (gamma); expected grey %d, %d, "
                "%d\n",
                linear.colour, glow.colour, kFitGlow, gamma.colour, ChannelLevel(displayed),
                ChannelLevel(glowCompensated), ChannelLevel(exposed));
    Check(ColourIsGrey(linear.colour, ChannelLevel(displayed)) &&
              ColourIsGrey(glow.colour, ChannelLevel(glowCompensated)) &&
              ColourIsGrey(gamma.colour, ChannelLevel(exposed)) && UsesLightingFogColour(linear.colour) &&
              UsesLightingFogColour(gamma.colour),
          "the fitted stock fog colour is the fog's own colour as the composite shows it: exposed, rolled off, "
          "gamma-encoded and glow-compensated in linear light, with the lighting colour's alpha");
}

void CheckFitAgainstTheVolumetricFog(const FogData& classic)
{
    const View v;
    Check(CheckFitFollows("homogeneous 0.002/yd", HomogeneousFog(kThinHomogeneousDensity, kEmissiveFog), v.in,
                          kThinHomogeneousLimits),
          "the fitted linear stock fog, applied through the client's planar-depth formula with the exponent forced "
          "to 1, follows the volumetric transmittance of homogeneous fog within 0.025 over 0..100 yd on the view axis "
          "and at the side of the view, closer at the side than a fit along the axis alone");
    Check(CheckFitFollows("ground fog 4 yd below the eye, falloff 1/18 yd", GroundFog(v.eye.z), v.in,
                          kGroundFogLimits),
          "the fitted stock fog follows ground fog within 0.03 on the view axis and 0.06 at the side of the view, "
          "closer at the side than a fit along the axis alone");
    const FrameInputs harbour = HarbourLevelView(v.world);
    AuthoredFog authored = {};
    const bool resolved =
        classic.Resolve(kEasternKingdoms, harbour.camPos, harbour.dayFraction, harbour.lightParams, authored);
    const FogParams harbourFog = BuildFogParams(harbour, Config(), resolved ? &authored : nullptr);
    Check(resolved && CheckFitFollows("Classic harbour sunset layers", harbourFog, harbour, kHarbourLimits),
          "the fitted stock fog follows the Classic harbour sunset layers within 0.03 on the view axis and 0.04 at "
          "the side of the view, closer at the side than a fit along the axis alone");
    const StockFogFit none = FitStockFog(EmptyFog(), v.in, {1.0f, 0.0f});
    Check(!none.fogs, "fog-free parameters fit no stock fog");
    CheckFittedColour();
}

bool SameArgs(const M2BatchFogArgs& a, const M2BatchFogArgs& b)
{
    return std::memcmp(&a, &b, sizeof(a)) == 0;
}

void CheckBatchFogKeepsBlendModeColours()
{
    const View v;
    const StockFogFit fit = FitStockFog(HomogeneousFog(kRegressionDensity, kEmissiveFog), v.in, {1.0f, 0.0f});
    bool blendModeColoursKept = fit.fogs;
    for (uint32_t colour : {kAdditiveFogColour, kModulateFogColour, kModulate2xFogColour})
    {
        M2BatchFogArgs args = {kPushedStockFogStart, kPushedStockFogEnd, kAuthoredClientExponent, &colour};
        ApplyStockFogFit(fit, args);
        blendModeColoursKept = blendModeColoursKept && args.colour == &colour && args.start == fit.start &&
                               args.end == fit.end && args.exponent == kLinearStockFogExponent;
    }
    const uint32_t lighting = kLightingColour;
    M2BatchFogArgs lit = {kPushedStockFogStart, kPushedStockFogEnd, kAuthoredClientExponent, &lighting};
    ApplyStockFogFit(fit, lit);
    Check(blendModeColoursKept && lit.colour == &fit.colour && *lit.colour == fit.colour &&
              lit.exponent == kLinearStockFogExponent,
          "an armed batch keeps the client's black, white and grey blend-mode fog colours, takes the fitted colour "
          "only for the lighting colour (alpha 0xFF), and gets the fitted start and end with the exponent 1");
    M2BatchFogArgs untouched = {kPushedStockFogStart, kPushedStockFogEnd, kAuthoredClientExponent, &lighting};
    const M2BatchFogArgs before = untouched;
    ApplyStockFogFit(FitStockFog(EmptyFog(), v.in, {1.0f, 0.0f}), untouched);
    Check(SameArgs(before, untouched), "a fit without fog leaves the client's pushed fog arguments bit-identical");
}

void CheckStockFogFit(const FogData& classic)
{
    CheckFitAgainstTheVolumetricFog(classic);
    CheckBatchFogKeepsBlendModeColours();
}

void CheckTransparentFogSetting(const std::wstring& outDir, const std::wstring& shippedIni)
{
    ConfigStore shipped;
    shipped.Load(NarrowPath(shippedIni));
    const std::string text = ReadText(shippedIni);
    const std::vector<std::string> lines = water_settings_checks::IniLines(text);
    Check(!shipped.Get().transparentFog && !Config().transparentFog &&
              std::count(lines.begin(), lines.end(), std::string("TransparentFog=0")) == 1 &&
              text.find("; 1 = fog particles, spell effects and other see-through models by their own distance") !=
                  std::string::npos,
          "the shipped INI and the built-in default keep TransparentFog=0 until an owner test, and the INI documents "
          "the key");

    const std::wstring savedIni = FullPath(outDir + L"\\transparent-fog.ini");
    CopyFileW(shippedIni.c_str(), savedIni.c_str(), FALSE);
    ConfigStore store;
    store.Load(NarrowPath(savedIni));
    Config edited = store.Get();
    edited.transparentFog = true;
    const std::string changes = SettingChanges(store.Get(), edited);
    store.Apply(edited);
    const bool saved = store.Save();
    ConfigStore reloaded;
    reloaded.Load(NarrowPath(savedIni));
    std::printf("     the settings window's TransparentFog change logs \"%s\"\n", changes.c_str());
    Check(saved && reloaded.Get().transparentFog && changes == "TransparentFog 0 -> 1" &&
              ReadText(savedIni).find("\nTransparentFog=1") != std::string::npos &&
              !SameFogSettings(reloaded.Get(), shipped.Get()),
          "a TransparentFog change is logged, saved to the INI, read back and counts as a fog setting change");
}

int g_glarePasses = 0;

void __cdecl RecordGlarePass()
{
    ++g_glarePasses;
}

struct RecordedFog
{
    float start;
    float end;
    float exponent;
    const uint32_t* colour;
    uint32_t colourValue;
};

RecordedFog g_recordedFog = {};

void __cdecl RecordM2BatchFog(float start, float end, float exponent, const uint32_t* colour)
{
    g_recordedFog = {start, end, exponent, colour, colour ? *colour : 0u};
}

struct ThunkCall
{
    uint32_t esi;
    uint32_t edi;
    uint32_t ebx;
    uint32_t stackBefore;
    uint32_t stackAfter;
};

ThunkCall CallThroughM2BatchFogThunk(const void* thunk, float start, float end, float exponent,
                                     const uint32_t* colour)
{
    uint32_t esiAfter = 0;
    uint32_t ediAfter = 0;
    uint32_t ebxAfter = 0;
    uint32_t stackBefore = 0;
    uint32_t stackAfter = 0;
    __asm {
        push esi
        push edi
        push ebx
        mov esi, kSentinelEsi
        mov edi, kSentinelEdi
        mov ebx, kSentinelEbx
        mov stackBefore, esp
        push colour
        push exponent
        push end
        push start
        call thunk
        add esp, kFogArgumentBytes
        mov stackAfter, esp
        mov esiAfter, esi
        mov ediAfter, edi
        mov ebxAfter, ebx
        pop ebx
        pop edi
        pop esi
    }
    return {esiAfter, ediAfter, ebxAfter, stackBefore, stackAfter};
}

bool CallerKept(const ThunkCall& call)
{
    return call.esi == kSentinelEsi && call.edi == kSentinelEdi && call.ebx == kSentinelEbx &&
           call.stackBefore == call.stackAfter;
}

void CallGlarePassThunk()
{
    reinterpret_cast<void(__cdecl*)()>(vf_test_glare_pass_thunk(reinterpret_cast<uintptr_t>(&RecordGlarePass)))();
}

struct ClientM2Shaders
{
    IDirect3DVertexShader9* vs = nullptr;
    IDirect3DPixelShader9* ps = nullptr;
    IDirect3DVertexDeclaration9* decl = nullptr;

    explicit ClientM2Shaders(IDirect3DDevice9* dev)
    {
        static const D3DVERTEXELEMENT9 elements[] = {
            {0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
            {0, 16, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
            D3DDECL_END(),
        };
        dev->CreateVertexShader(reinterpret_cast<const DWORD*>(g_vs_client_m2_fog), &vs);
        dev->CreatePixelShader(reinterpret_cast<const DWORD*>(g_ps_client_m2_fog), &ps);
        dev->CreateVertexDeclaration(elements, &decl);
    }

    ~ClientM2Shaders()
    {
        IUnknown* objects[] = {vs, ps, decl};
        for (IUnknown* object : objects)
            if (object)
                object->Release();
    }

    ClientM2Shaders(const ClientM2Shaders&) = delete;
    ClientM2Shaders& operator=(const ClientM2Shaders&) = delete;

    bool Ready() const { return vs && ps && decl; }
};

void DrawClientAdditiveEffect(IDirect3DDevice9* dev, const ClientM2Shaders& shaders, const View& v,
                              const M2BatchFogArgs& fog)
{
    struct EffectVertex
    {
        float x, y, z, w;
        DWORD colour;
    };
    const DWORD colour = D3DCOLOR_ARGB(0xFF, kEffectLevel, kEffectLevel, kEffectLevel);
    const float z = kEffectDepth;
    const float s = kEffectHalfSize;
    const EffectVertex quad[6] = {{-s, -s, z, 1, colour}, {s, -s, z, 1, colour}, {-s, s, z, 1, colour},
                                  {s, -s, z, 1, colour},  {s, s, z, 1, colour},  {-s, s, z, 1, colour}};
    const ClientFogConstant c30 = ClientM2FogConstant(fog.start, fog.end, fog.exponent);
    const float viewDepthRow[4] = {0.0f, 0.0f, 1.0f, 0.0f};
    const float fogColour[4] = {ColourChannel(*fog.colour, 16), ColourChannel(*fog.colour, 8),
                                ColourChannel(*fog.colour, 0), 0.0f};
    dev->SetVertexShader(shaders.vs);
    dev->SetPixelShader(shaders.ps);
    dev->SetVertexDeclaration(shaders.decl);
    dev->SetVertexShaderConstantF(0, v.d3dProj, 4);
    dev->SetVertexShaderConstantF(30, &c30.x, 1);
    dev->SetVertexShaderConstantF(33, viewDepthRow, 1);
    dev->SetPixelShaderConstantF(2, fogColour, 1);
    dev->SetViewport(&v.world);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
    dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
    dev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
    dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
    dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, quad, sizeof(EffectVertex));
    dev->SetVertexShader(nullptr);
    dev->SetPixelShader(nullptr);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
}

void DrawBackdrop(Harness& h, const View& v, float viewDepth, DWORD colour)
{
    IDirect3DDevice9* dev = h.dev;
    dev->SetViewport(&v.world);
    dev->SetVertexShader(nullptr);
    dev->SetPixelShader(nullptr);
    dev->SetTexture(0, nullptr);
    dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    dev->SetRenderState(D3DRS_LIGHTING, FALSE);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
    dev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
    h.DrawPretransformedQuadAtRawDepth(0.0f, 0.0f, static_cast<float>(v.world.Width),
                                       static_cast<float>(v.world.Height), v.RawDepthAt(viewDepth), colour);
}

enum class Backdrop
{
    Scene,
    FarWall,
};

struct FrameOptions
{
    Backdrop backdrop = Backdrop::Scene;
    bool drawEffect = false;
    bool captureAfterLiquid = false;
    bool keepStateAround = false;
    bool unbindDepthAtLiquidEnd = false;
    bool readDepthAtLiquidEnd = false;
    const DepthTexel* depthTexels = nullptr;
    void (*drawOverBackdrop)(Harness& h) = nullptr;
};

struct HookedFrame
{
    Image image;
    Image afterLiquid;
    M2BatchFogArgs effectFog = {};
    bool effectColourKept = false;
    uint32_t effectFogColour = 0;
    int glarePassesBeforeTheFog = 0;
    int glarePassesAtOwnCall = 0;
    bool stateKept = true;
    engine::StockFog stockFogInFrame = {};
    engine::StockFog stockFogAfterFrame = {};
    float depthAtLiquidEnd[2] = {};
    bool depthRead = false;
};

void DrawBackdropFor(Harness& h, const View& v, const FrameOptions& options)
{
    if (options.backdrop == Backdrop::FarWall)
        DrawBackdrop(h, v, kBackgroundDepth, kBackgroundColour);
    else
        h.DrawScene(v.eye, v.view, v.proj, v.world);
    if (options.drawOverBackdrop)
        options.drawOverBackdrop(h);
}

bool KeepsState(Harness& h, const View& v, void (*entry)())
{
    h.SetEngineState(v.world);
    Sentinel before;
    ReadSentinel(h.dev, before);
    entry();
    Sentinel after;
    ReadSentinel(h.dev, after);
    const bool kept = SameSentinel(before, after);
    if (!kept)
        ReportSentinelDifferences(before, after);
    ReleaseSentinel(before);
    ReleaseSentinel(after);
    return kept;
}

void EndLiquidWithDepthUnbound(Harness& h)
{
    IDirect3DSurface9* bound = nullptr;
    h.dev->GetDepthStencilSurface(&bound);
    h.dev->SetDepthStencilSurface(nullptr);
    vf_test_hook_liquid_end();
    h.dev->SetDepthStencilSurface(bound);
    if (bound)
        bound->Release();
}

void EndLiquid(Harness& h, const View& v, const FrameOptions& options, HookedFrame& frame)
{
    const int before = g_glarePasses;
    if (options.unbindDepthAtLiquidEnd)
        EndLiquidWithDepthUnbound(h);
    else if (options.keepStateAround)
        frame.stateKept = KeepsState(h, v, &vf_test_hook_liquid_end) && frame.stateKept;
    else
        vf_test_hook_liquid_end();
    frame.glarePassesBeforeTheFog = g_glarePasses - before;
    if (options.readDepthAtLiquidEnd)
        frame.depthRead = vf_test_read_scene_depth(options.depthTexels, 2, frame.depthAtLiquidEnd) != 0;
    if (options.captureAfterLiquid)
        frame.afterLiquid = Capture(h.dev);
}

void DrawEffect(Harness& h, const View& v, const FrameOptions& options, const ClientM2Shaders* shaders,
                HookedFrame& frame)
{
    const uint32_t additive = kAdditiveFogColour;
    frame.effectFog = {kPushedStockFogStart, kPushedStockFogEnd, kAuthoredClientExponent, &additive};
    vf_test_hook_m2_batch_fog(&frame.effectFog);
    frame.effectColourKept = frame.effectFog.colour == &additive;
    frame.effectFogColour = *frame.effectFog.colour;
    if (options.drawEffect && shaders && shaders->Ready())
        DrawClientAdditiveEffect(h.dev, *shaders, v, frame.effectFog);
    frame.effectFog.colour = nullptr;
}

HookedFrame RenderHookedFrame(Harness& h, const View& v, const Config& cfg, const FrameOptions& options,
                              const ClientM2Shaders* shaders = nullptr)
{
    HookedFrame frame;
    vf_test_set_config(&cfg);
    vf_test_glare_pass_thunk(reinterpret_cast<uintptr_t>(&RecordGlarePass));
    vf_test_use_fog_hook_client(&v.in);
    vf_test_hook_frame_begin();
    vf_test_hook_stock_fog(&frame.stockFogInFrame, nullptr);
    h.BeginFrame();
    h.dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    DrawBackdropFor(h, v, options);
    EndLiquid(h, v, options, frame);
    DrawEffect(h, v, options, shaders, frame);
    const int beforeOwnCall = g_glarePasses;
    CallGlarePassThunk();
    frame.glarePassesAtOwnCall = g_glarePasses - beforeOwnCall;
    if (options.keepStateAround)
        frame.stateKept = KeepsState(h, v, &vf_test_hook_world_done) && frame.stateKept;
    else
        vf_test_hook_world_done();
    frame.image = Capture(h.dev);
    h.dev->EndScene();
    vf_test_hook_frame_end();
    vf_test_hook_stock_fog(&frame.stockFogAfterFrame, nullptr);
    h.dev->Present(nullptr, nullptr, nullptr, nullptr);
    return frame;
}

HookedFrame RenderSettledHookedFrame(Harness& h, const View& v, const Config& cfg, const FrameOptions& options,
                                     const ClientM2Shaders* shaders = nullptr)
{
    HookedFrame frame;
    for (int i = 0; i < kWarmUpFrames; ++i)
        frame = RenderHookedFrame(h, v, cfg, options, shaders);
    return frame;
}

Image RenderWholeFrameDirectly(Harness& h, const View& v, const Config& cfg, const FrameOptions& options)
{
    vf_test_set_config(&cfg);
    h.BeginFrame();
    h.dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    DrawBackdropFor(h, v, options);
    const char* skip = "";
    vf_test_render(&v.in, &skip);
    Image image = Capture(h.dev);
    h.dev->EndScene();
    h.dev->Present(nullptr, nullptr, nullptr, nullptr);
    return image;
}

int LargestDifference(const Image& a, const Image& b, const D3DVIEWPORT9& region)
{
    int largest = 0;
    for (UINT y = region.Y; y < region.Y + region.Height; ++y)
        for (UINT x = region.X; x < region.X + region.Width; ++x)
            for (int c = 0; c < 3; ++c)
                largest = std::max(largest, std::abs(static_cast<int>(a.At(x, y)[c]) - b.At(x, y)[c]));
    return largest;
}

int LargestDarkening(const Image& before, const Image& after, const D3DVIEWPORT9& region)
{
    int largest = 0;
    for (UINT y = region.Y; y < region.Y + region.Height; ++y)
        for (UINT x = region.X; x < region.X + region.Width; ++x)
            for (int c = 0; c < 3; ++c)
                largest = std::max(largest, static_cast<int>(before.At(x, y)[c]) - after.At(x, y)[c]);
    return largest;
}

Config HookConfig(bool transparentFog)
{
    Config cfg;
    cfg.overlay = false;
    cfg.temporal = 0.0f;
    cfg.noiseAmount = 0.0f;
    cfg.godRays = 0.0f;
    cfg.dataMode = 0;
    cfg.transparentFog = transparentFog;
    return cfg;
}

bool PassedThrough(const HookedFrame& frame)
{
    return frame.effectFog.start == kPushedStockFogStart && frame.effectFog.end == kPushedStockFogEnd &&
           frame.effectFog.exponent == kAuthoredClientExponent && frame.effectColourKept;
}

bool Rewritten(const HookedFrame& frame)
{
    return frame.effectFog.start != kPushedStockFogStart && frame.effectFog.end != kPushedStockFogEnd &&
           frame.effectFog.exponent == kLinearStockFogExponent && frame.effectColourKept &&
           frame.effectFogColour == kAdditiveFogColour;
}

void CheckEarlyCompositeMatchesWholeFrame(Harness& h)
{
    const View v;
    FrameOptions options;
    options.captureAfterLiquid = true;
    engine::StockFog clientFog = {{kStockFogStart, kStockFogStart}, {kStockFogEnd, kStockFogEnd}};
    vf_test_hook_stock_fog(nullptr, &clientFog);
    const HookedFrame whole = RenderSettledHookedFrame(h, v, HookConfig(false), options);
    const HookedFrame early = RenderSettledHookedFrame(h, v, HookConfig(true), options);
    const Image direct = RenderWholeFrameDirectly(h, v, HookConfig(false), options);
    const int wholeVsDirect = LargestDifference(whole.image, direct, v.world);
    const int earlyVsWhole = LargestDifference(early.image, whole.image, v.world);
    const int earlyAtLiquidEnd = LargestDifference(early.afterLiquid, whole.image, v.world);
    std::printf("     hooked frames: TransparentFog=0 vs the fog pass alone %d/255, TransparentFog=1 vs 0 %d/255 "
                "(%d/255 already at the liquid end); glare drawn before the fog %d/%d, at its own call %d/%d\n",
                wholeVsDirect, earlyVsWhole, earlyAtLiquidEnd, whole.glarePassesBeforeTheFog,
                early.glarePassesBeforeTheFog, whole.glarePassesAtOwnCall, early.glarePassesAtOwnCall);
    Check(wholeVsDirect == 0 && PassedThrough(whole) && whole.glarePassesBeforeTheFog == 0 &&
              whole.glarePassesAtOwnCall == 1,
          "with TransparentFog=0 the hooks pass through: the fog draws once after the world as before, the M2 batch "
          "fog arguments stay bit-identical and the glare draws at its own call");
    Check(earlyVsWhole <= kMaxEarlyDifference && earlyAtLiquidEnd <= kMaxEarlyDifference,
          "with TransparentFog=1 the early composite at the liquid end fogs opaque pixels as the single composite "
          "after the world does, and the end of the world adds nothing without god rays");
    Check(Rewritten(early) && early.glarePassesBeforeTheFog == 1 && early.glarePassesAtOwnCall == 0,
          "after the early composite the M2 batch fog is armed and the glare pass is drawn once, before the fog, "
          "and skipped at its own call");
    const bool pushed = early.stockFogInFrame.start[0] == kPushedStockFogStart &&
                        early.stockFogInFrame.end[1] == kPushedStockFogEnd;
    const bool restored = early.stockFogAfterFrame.start[0] == kStockFogStart &&
                          early.stockFogAfterFrame.end[1] == kStockFogEnd;
    Check(pushed && restored, "the stock fog stays pushed out of range for the world render and is restored after");
}

struct OwnDepthResult
{
    float levelChange;
    float factor;
    bool rewritten;
};

OwnDepthResult MeasureEffectAtOwnDepth(Harness& h, const View& v, bool transparentFog,
                                       const ClientM2Shaders& shaders)
{
    FrameOptions withEffect;
    withEffect.backdrop = Backdrop::FarWall;
    withEffect.drawEffect = true;
    FrameOptions withoutEffect = withEffect;
    withoutEffect.drawEffect = false;
    const Config cfg = HookConfig(transparentFog);
    const HookedFrame lit = RenderSettledHookedFrame(h, v, cfg, withEffect, &shaders);
    const HookedFrame dark = RenderSettledHookedFrame(h, v, cfg, withoutEffect, &shaders);
    const int cx = static_cast<int>(v.world.X + v.world.Width / 2);
    const int cy = static_cast<int>(v.world.Y + v.world.Height / 2);
    const Rgb a = SampleRgb(lit.image, cx, cy, kProbeRadius);
    const Rgb b = SampleRgb(dark.image, cx, cy, kProbeRadius);
    const ClientFogConstant c30 =
        ClientM2FogConstant(lit.effectFog.start, lit.effectFog.end, lit.effectFog.exponent);
    return {(a.r - b.r + a.g - b.g + a.b - b.b) / 3.0f, ClientM2FogFactor(c30, kEffectDepth), Rewritten(lit)};
}

void CheckTransparentsFoggedAtOwnDepth(Harness& h)
{
    ClientM2Shaders shaders(h.dev);
    const View v;
    const FogParams fog = HomogeneousFog(kRegressionDensity, kRegressionEmissive);
    vf_test_force_fog_params(&fog);
    const OwnDepthResult before = MeasureEffectAtOwnDepth(h, v, false, shaders);
    const OwnDepthResult after = MeasureEffectAtOwnDepth(h, v, true, shaders);
    vf_test_force_fog_params(nullptr);
    const float transmittance = std::exp(-kRegressionDensity * kEffectDepth);
    const float background = std::exp(-kRegressionDensity * kBackgroundDepth);
    const float expected = kEffectLevel * transmittance;
    std::printf("     additive effect %d/255 at %.0f yd over a wall at %.0f yd (T %.3f and %.3f): adds %.1f/255 with "
                "TransparentFog=0, %.1f/255 with TransparentFog=1 (client fog factor %.3f), expected %.1f/255\n",
                kEffectLevel, kEffectDepth, kBackgroundDepth, transmittance, background, before.levelChange,
                after.levelChange, after.factor, expected);
    Check(shaders.Ready() && after.rewritten && std::fabs(after.factor - transmittance) < kMaxFactorError &&
              std::fabs(after.levelChange - expected) <= kMaxOwnDepthLevelError,
          "with TransparentFog=1 an additive effect drawn after the early composite through the client's M2 fog "
          "formula is fogged at its own depth (effect x T(10 yd)), not at the wall's");
    Check(!before.rewritten && before.levelChange < expected * 0.5f,
          "with TransparentFog=0 the same effect keeps today's single composite and takes the fog of the wall "
          "behind it");
}

void CheckGodRaysAfterTheWorld(Harness& h)
{
    const View v;
    FrameOptions options;
    options.captureAfterLiquid = true;
    Config cfg = HookConfig(true);
    cfg.godRays = kRaysOn;
    const HookedFrame rays = RenderSettledHookedFrame(h, v, cfg, options);
    float sunDir[3];
    TransformDirection(v.in.toLight, v.view, sunDir);
    const float sunX = v.world.X + (sunDir[0] / sunDir[2] * v.proj[0] * 0.5f + 0.5f) * v.world.Width;
    const float sunY = v.world.Y + (0.5f - sunDir[1] / sunDir[2] * v.proj[5] * 0.5f) * v.world.Height;
    const UINT x0 = static_cast<UINT>(std::fmax(sunX - kRaySunRegion, 0.0f));
    const UINT y0 = static_cast<UINT>(std::fmax(sunY - kRaySunRegion, 0.0f));
    const UINT x1 = static_cast<UINT>(std::fmin(sunX + kRaySunRegion, static_cast<float>(v.world.Width)));
    const UINT y1 = static_cast<UINT>(std::fmin(sunY + kRaySunRegion, static_cast<float>(v.world.Height)));
    const double gain = MeanLumaChange(rays.afterLiquid, rays.image, x0, y0, x1, y1);
    const int darkening = LargestDarkening(rays.afterLiquid, rays.image, v.world);
    Config wholeFrameCfg = HookConfig(false);
    wholeFrameCfg.godRays = kRaysOn;
    const HookedFrame wholeFrameRays = RenderSettledHookedFrame(h, v, wholeFrameCfg, options);
    const int wholeFrameDifference = LargestDifference(rays.image, wholeFrameRays.image, v.world);
    std::printf("     god rays after the early composite: mean luma change %.4f around the sun (%.0f, %.0f), largest "
                "darkening %d/255, largest difference from the single composite's god rays %d/255\n",
                gain, sunX, sunY, darkening, wholeFrameDifference);
    Check(gain > kMinRayGain && darkening <= kMaxRayDarkening,
          "with TransparentFog=1 the god rays are added over the finished world at the end of the world render and "
          "darken nothing");
    Check(wholeFrameDifference <= kMaxLateRayDifference,
          "the god rays drawn after the early composite come from the scene before the fog, as the single "
          "composite's do, and match them");
}

void CheckHookedFogRestoresState(Harness& h)
{
    const View v;
    FrameOptions options;
    options.keepStateAround = true;
    Config cfg = HookConfig(true);
    cfg.godRays = kRaysOn;
    const HookedFrame early = RenderSettledHookedFrame(h, v, cfg, options);
    const HookedFrame whole = RenderSettledHookedFrame(h, v, HookConfig(false), options);
    Check(early.stateKept && whole.stateKept && Rewritten(early),
          "the early composite at the liquid end, the late god rays and the single composite restore render, "
          "sampler, shader, constant, stream, viewport, scissor and target state");
}

struct Fallback
{
    const char* name;
    Config cfg;
    bool cameraInLiquid;
    bool unbindDepth;
};

void CheckWholeFrameFallbacks(Harness& h)
{
    Config debugView = HookConfig(true);
    debugView.debugView = 2;
    Config sunMarker = HookConfig(true);
    sunMarker.sunMarker = true;
    Config stockFogKept = HookConfig(true);
    stockFogKept.stockFog = 0;
    Config underwater = HookConfig(true);
    underwater.underwater = true;
    const Fallback fallbacks[] = {
        {"DebugView=2", debugView, false, false},
        {"SunMarker=1", sunMarker, false, false},
        {"StockFog=0", stockFogKept, false, false},
        {"camera under water with Underwater=1", underwater, true, false},
        {"early composite failed (depth not bound)", HookConfig(true), false, true},
    };
    bool allFallBack = true;
    for (const Fallback& f : fallbacks)
    {
        View v;
        v.in.inLiquid = f.cameraInLiquid;
        FrameOptions options;
        options.unbindDepthAtLiquidEnd = f.unbindDepth;
        const HookedFrame frame = RenderSettledHookedFrame(h, v, f.cfg, options);
        const Image direct = RenderWholeFrameDirectly(h, v, f.cfg, options);
        const int difference = LargestDifference(frame.image, direct, v.world);
        const int glareDraws = frame.glarePassesBeforeTheFog + frame.glarePassesAtOwnCall;
        const bool fellBack = PassedThrough(frame) && glareDraws == 1 && difference == 0;
        std::printf("     %s: M2 fog %s, glare drawn %d before the fog and %d at its own call, frame vs the fog "
                    "pass alone %d/255\n",
                    f.name, PassedThrough(frame) ? "passed through" : "rewritten", frame.glarePassesBeforeTheFog,
                    frame.glarePassesAtOwnCall, difference);
        allFallBack = allFallBack && fellBack;
    }
    Check(allFallBack,
          "with TransparentFog=1 a debug view, the sun marker, StockFog=0, a camera under water or a failed early "
          "composite fall back to the single composite after the world with the M2 batch fog unarmed and one glare "
          "draw");
}

void CheckThunksAndGlarePass(Harness& h)
{
    const void* thunk = vf_test_m2_batch_fog_thunk(reinterpret_cast<uintptr_t>(&RecordM2BatchFog));
    const uint32_t lighting = kLightingColour;
    g_recordedFog = {};
    const ThunkCall unarmed = CallThroughM2BatchFogThunk(thunk, kPushedStockFogStart, kPushedStockFogEnd,
                                                         kAuthoredClientExponent, &lighting);
    const bool unarmedPassed = g_recordedFog.start == kPushedStockFogStart &&
                               g_recordedFog.end == kPushedStockFogEnd &&
                               g_recordedFog.exponent == kAuthoredClientExponent && g_recordedFog.colour == &lighting;

    const View v;
    Config cfg = HookConfig(true);
    vf_test_set_config(&cfg);
    RenderSettledHookedFrame(h, v, cfg, FrameOptions());
    vf_test_glare_pass_thunk(reinterpret_cast<uintptr_t>(&RecordGlarePass));
    vf_test_use_fog_hook_client(&v.in);
    vf_test_hook_frame_begin();
    h.BeginFrame();
    h.DrawScene(v.eye, v.view, v.proj, v.world);
    const int glareBefore = g_glarePasses;
    vf_test_hook_liquid_end();
    CallGlarePassThunk();
    const int glareDraws = g_glarePasses - glareBefore;
    g_recordedFog = {};
    const ThunkCall armed = CallThroughM2BatchFogThunk(thunk, kPushedStockFogStart, kPushedStockFogEnd,
                                                       kAuthoredClientExponent, &lighting);
    const RecordedFog armedFog = g_recordedFog;
    vf_test_hook_world_done();
    h.dev->EndScene();
    vf_test_hook_frame_end();
    h.dev->Present(nullptr, nullptr, nullptr, nullptr);
    g_recordedFog = {};
    CallThroughM2BatchFogThunk(thunk, kPushedStockFogStart, kPushedStockFogEnd, kAuthoredClientExponent, &lighting);
    const bool disarmedAfterFrame = g_recordedFog.start == kPushedStockFogStart && g_recordedFog.colour == &lighting;
    const int glareAfterFrame = g_glarePasses;
    vf_test_hook_frame_begin();
    CallGlarePassThunk();
    const bool glarePassesNextFrame = g_glarePasses == glareAfterFrame + 1;
    vf_test_hook_frame_end();
    const bool armedRewritten = armedFog.start != kPushedStockFogStart &&
                                armedFog.exponent == kLinearStockFogExponent && armedFog.colour != &lighting &&
                                UsesLightingFogColour(armedFog.colourValue);
    std::printf("     M2 batch fog thunk: unarmed start %.0f, armed start %.1f end %.1f exponent %.2f colour %08X\n",
                kPushedStockFogStart, armedFog.start, armedFog.end, armedFog.exponent, armedFog.colourValue);
    Check(unarmedPassed && armedRewritten && disarmedAfterFrame && CallerKept(unarmed) && CallerKept(armed),
          "the M2 batch fog thunk forwards the caller's cdecl arguments unchanged while unarmed, substitutes the "
          "fitted fog while armed, disarms at the end of the frame, and keeps esi, edi, ebx and the stack");
    Check(glareDraws == 1 && glarePassesNextFrame,
          "the glare thunk skips its call when the glare already drew before the fog, and calls the glare pass "
          "again in the next frame");
}

void CheckTransparentFog(Harness& h)
{
    Config saved;
    vf_test_get_config(&saved);
    CheckEarlyCompositeMatchesWholeFrame(h);
    CheckTransparentsFoggedAtOwnDepth(h);
    CheckGodRaysAfterTheWorld(h);
    CheckHookedFogRestoresState(h);
    CheckWholeFrameFallbacks(h);
    CheckThunksAndGlarePass(h);
    vf_test_set_config(&saved);
}

void CheckEarlyCompositeOnMultisampledDevice(Harness& m, const D3DVIEWPORT9& world, const DepthTexel* texels,
                                             const float* drawnDepth, void (*drawDepthQuads)(Harness& h))
{
    const View v(Add({0, 0, 9}, kGameLikeWorldOffset), Add({100, 2, 4}, kGameLikeWorldOffset), world);
    FrameOptions withoutQuads;
    FrameOptions withQuads;
    withQuads.drawOverBackdrop = drawDepthQuads;
    withQuads.readDepthAtLiquidEnd = true;
    withQuads.depthTexels = texels;
    const HookedFrame whole = RenderSettledHookedFrame(m, v, HookConfig(false), withQuads);
    RenderHookedFrame(m, v, HookConfig(true), withoutQuads);
    const HookedFrame early = RenderHookedFrame(m, v, HookConfig(true), withQuads);
    const int difference = LargestDifference(early.image, whole.image, v.world);
    const bool copied = early.depthRead && std::fabs(early.depthAtLiquidEnd[0] - drawnDepth[0]) <= 1e-6f &&
                        std::fabs(early.depthAtLiquidEnd[1] - drawnDepth[1]) <= 1e-6f;
    std::printf("     4x hooked frames: TransparentFog=1 vs 0 %d/255; depth copied by the liquid end %.7f / %.7f "
                "(drawn %.2f / %.2f after a frame without them)\n",
                difference, early.depthAtLiquidEnd[0], early.depthAtLiquidEnd[1], drawnDepth[0], drawnDepth[1]);
    Check(copied && Rewritten(early) && difference <= kMaxMultisampledEarlyDifference,
          "on a 4x device the early composite copies this frame's multisampled depth before it fogs, arms the M2 "
          "batch fog and matches the single composite after the world");
}
}
