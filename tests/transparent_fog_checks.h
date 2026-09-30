#pragma once

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
}
