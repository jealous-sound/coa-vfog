#pragma once

namespace authored_fog
{
constexpr uint32_t kHyjalLight = 265;
const float kHyjalSummit[3] = {5458.0f, -2934.0f, 1481.0f};
const float kHarbourAtStormwindLightEdge[3] = {-8565.21f, 993.46f, 104.96f};
const float kStormwindLightCentre[3] = {-8801.1f, 578.1f, 0.0f};
const float kLochModanLight23Centre[3] = {-6314.7f, 469.3f, 192.0f};
const float kBlastedLandsLight19Centre[3] = {-12041.6f, -2450.3f, 20.0f};
constexpr float kMidnight = 0.0f;
constexpr float kDayFraction1730 = 2100.0f / 2880.0f;
constexpr int kLegacyGlowSlot = 1;
constexpr float kLegacyParams37Glow = 1.0f;
constexpr float kDeathSlotGlowOfLight1 = 0.8f;
constexpr float kDeathSlotGlowOfLight77 = 0.6f;
constexpr int kCurveMidInput = 16;
constexpr int kCurveHighInput = 24;
constexpr float kCurveCodeMax = 255.0f;
constexpr float kCurve8286666Mid = 144.0f / kCurveCodeMax;
constexpr float kCurve8286666High = 227.0f / kCurveCodeMax;
constexpr float kCurve1140733Mid = 131.0f / kCurveCodeMax;
constexpr float kCurve8248426Mid = 151.0f / kCurveCodeMax;
constexpr float kCurve8248426High = 239.0f / kCurveCodeMax;
constexpr int kBlackwingLair = 469;
constexpr uint32_t kBlackwingLairLight = 83;
constexpr float kBlackwingLairParams85Glow = 0.9f;
const float kInsideBlackwingLair[3] = {-7520.0f, -1050.0f, 450.0f};

bool NearVector(const float* v, float x, float y, float z)
{
    return Near(v[0], x) && Near(v[1], y) && Near(v[2], z);
}

float IdentityCurve(int input)
{
    return static_cast<float>(input) / (kGradingCurveEntries - 1);
}

void CheckHyjalNoise(const FogData& data)
{
    AuthoredFog fog = {};
    const bool resolved = data.Resolve(kKalimdor, kHyjalSummit, kMidnight, kClearWeather, fog);
    const AuthoredNoise& haze = fog.layers[0].noise;
    std::printf("     Hyjal 00:00 layer 0 noise: presence %.2f, tiles %.0f/%.0f yd, velocities (%.1f %.1f %.1f) "
                "(%.1f %.1f %.1f) yd/s; layers 1-2 presence %.2f %.2f\n",
                haze.presence, haze.tileYards[0], haze.tileYards[1], haze.velocity[0][0], haze.velocity[0][1],
                haze.velocity[0][2], haze.velocity[1][0], haze.velocity[1][1], haze.velocity[1][2],
                fog.layers[1].noise.presence, fog.layers[2].noise.presence);
    Check(resolved && fog.lightCount == 1 && fog.lightIds[0] == kHyjalLight && haze.presence == 1.0f &&
              Near(haze.octaveShare[0], 1.0f) && Near(haze.octaveShare[1], 1.0f) &&
              Near(haze.tileYards[0], 5000.0f) && Near(haze.tileYards[1], 5000.0f) &&
              NearVector(haze.velocity[0], 15.0f, 15.0f, 15.0f) &&
              NearVector(haze.velocity[1], -30.0f, -30.0f, -30.0f) && fog.layers[1].noise.presence == 0.0f &&
              fog.layers[2].noise.presence == 0.0f,
          "Hyjal carries its authored two-octave noise on the haze layer only");
}

void CheckStormNoiseFollowsTheStormWeight(const FogData& data)
{
    const float blends[] = {0.0f, 0.49f, 0.5f, 0.51f, 1.0f};
    AuthoredNoise noise[5] = {};
    bool resolved = true;
    for (int i = 0; i < 5; ++i)
    {
        AuthoredFog fog = {};
        resolved = data.Resolve(kEasternKingdoms, kOpenSeaWestOfElwynn, kNoon, Storm(blends[i]), fog) && resolved;
        noise[i] = fog.layers[2].noise;
    }
    const AuthoredNoise& half = noise[2];
    std::printf("     open sea at noon, near-fog noise presence by storm weight: %.2f %.2f %.2f %.2f %.2f; half storm "
                "tiles %.0f/%.0f yd\n",
                noise[0].presence, noise[1].presence, noise[2].presence, noise[3].presence, noise[4].presence,
                half.tileYards[0], half.tileYards[1]);
    Check(resolved && noise[0].presence == 0.0f && Near(noise[4].presence, 1.0f) && Near(half.presence, 0.5f) &&
              std::fabs(noise[3].presence - noise[1].presence) < 0.03f,
          "storm noise fades in with the storm weight instead of switching at half weight");
    Check(Near(half.tileYards[0], 300.0f) && Near(half.tileYards[1], 300.0f) &&
              NearVector(half.velocity[0], 0.5f, 0.5f, 0.0f) && NearVector(half.velocity[1], -1.0f, -1.0f, 0.0f),
          "blended noise keeps the scale and drift of the layers that carry it");
}

void CheckClassicGlow(const FogData& data)
{
    AuthoredFog clear = {};
    AuthoredFog ghost = {};
    AuthoredFog legacy = {};
    const bool resolved =
        data.Resolve(kEasternKingdoms, kHarbourAtStormwindLightEdge, kDayFraction1800, kClearWeather, clear) &&
        data.Resolve(kEasternKingdoms, kHarbourAtStormwindLightEdge, kDayFraction1800,
                     ScreenEffectSlot(FogData::kDeathSlot, 0.0f), ghost) &&
        data.Resolve(kEasternKingdoms, kBlastedLandsLight19Centre, kNoon, ScreenEffectSlot(kLegacyGlowSlot, 0.0f),
                     legacy);
    const float expectedGhost = WeightOf(ghost, kEasternKingdomsGlobalLight) * kDeathSlotGlowOfLight1 +
                                WeightOf(ghost, kStormwindLight) * kDeathSlotGlowOfLight77;
    std::printf("     glow: harbour clear %.3f, ghost %.3f (expected %.3f); Blasted Lands slot 1 %.3f with %d layers\n",
                clear.glow, ghost.glow, expectedGhost, legacy.glow, legacy.layerCount);
    Check(resolved && clear.hasGlow && clear.glow == 0.0f && ghost.hasGlow && Near(ghost.glow, expectedGhost),
          "Classic glow blends by light weight in the selected light params slot");
    Check(legacy.hasGlow && Near(legacy.glow, kLegacyParams37Glow) && legacy.layerCount == 0,
          "light params without fog still supply their glow");
}

bool IsIdentityCurve(const float* curve)
{
    for (int input = 0; input < kGradingCurveEntries; ++input)
        if (!Near(curve[input], IdentityCurve(input)))
            return false;
    return true;
}

void CheckGlowWithoutClassicFog(const FogData& data)
{
    AuthoredFog lair = {};
    AuthoredFog outland = {};
    const bool lairFog = data.Resolve(kBlackwingLair, kInsideBlackwingLair, kNoon, kClearWeather, lair);
    const bool outlandFog = data.Resolve(kOutland, kInsideBlackwingLair, kNoon, kClearWeather, outland);
    std::printf("     Blackwing Lair: Classic fog %d, lights %d (first %u), coverage %.2f, glow %.2f (%s), grading %s\n",
                lairFog, lair.lightCount, lair.lightCount > 0 ? lair.lightIds[0] : 0u, lair.coverage, lair.glow,
                lair.hasGlow ? "resolved" : "none", lair.hasGradingCurve ? "resolved" : "none");
    Check(!lairFog && lair.layerCount == 0 && lair.lightCount == 1 && lair.lightIds[0] == kBlackwingLairLight &&
              Near(lair.coverage, 1.0f) && lair.hasGlow && Near(lair.glow, kBlackwingLairParams85Glow) &&
              IsIdentityCurve(lair.gradingCurve) && !lair.hasGradingCurve,
          "a map without Classic fog keeps the derived fog but still resolves its Classic lights' glow and grading, "
          "here a light without a grading key, so no grading curve");
    Check(!outlandFog && outland.lightCount == 0 && !outland.hasGlow && IsIdentityCurve(outland.gradingCurve) &&
              !outland.hasGradingCurve,
          "a map without Classic lights resolves no glow, no grading curve and the identity");
}

void CheckGradingCurves(const FogData& data)
{
    AuthoredFog noon = {};
    AuthoredFog midnight = {};
    AuthoredFog dusk = {};
    AuthoredFog halfAfterDusk = {};
    AuthoredFog identityKey = {};
    AuthoredFog halfStorm = {};
    const bool resolved =
        data.Resolve(kEasternKingdoms, kStormwindLightCentre, kNoon, kClearWeather, noon) &&
        data.Resolve(kEasternKingdoms, kStormwindLightCentre, kMidnight, kClearWeather, midnight) &&
        data.Resolve(kEasternKingdoms, kLochModanLight23Centre, kDayFraction1800, kClearWeather, dusk) &&
        data.Resolve(kEasternKingdoms, kLochModanLight23Centre, kDayFraction1730, kClearWeather, halfAfterDusk) &&
        data.Resolve(kEasternKingdoms, kLochModanLight23Centre, kNoon, kClearWeather, identityKey) &&
        data.Resolve(kEasternKingdoms, kOpenSeaWestOfElwynn, kNoon, Storm(0.5f), halfStorm);
    std::printf("     grading at inputs 16/31, 24/31: Stormwind noon %.3f %.3f, midnight %.3f %.3f; light 23 18:00 "
                "%.3f %.3f, 17:30 %.3f, 12:00 %.3f; open sea half storm %.3f\n",
                noon.gradingCurve[kCurveMidInput], noon.gradingCurve[kCurveHighInput],
                midnight.gradingCurve[kCurveMidInput], midnight.gradingCurve[kCurveHighInput],
                dusk.gradingCurve[kCurveMidInput], dusk.gradingCurve[kCurveHighInput],
                halfAfterDusk.gradingCurve[kCurveMidInput], identityKey.gradingCurve[kCurveMidInput],
                halfStorm.gradingCurve[kCurveMidInput]);
    Check(resolved && Near(noon.gradingCurve[kCurveMidInput], kCurve8286666Mid) &&
              Near(noon.gradingCurve[kCurveHighInput], kCurve8286666High) &&
              Near(midnight.gradingCurve[kCurveMidInput], kCurve8286666Mid) &&
              Near(midnight.gradingCurve[kCurveHighInput], kCurve8286666High),
          "a grading LUT set only on the noon key holds all day");
    Check(Near(dusk.gradingCurve[kCurveMidInput], kCurve8248426Mid) &&
              Near(dusk.gradingCurve[kCurveHighInput], kCurve8248426High) &&
              Near(halfAfterDusk.gradingCurve[kCurveMidInput], 0.5f * (kCurve1140733Mid + kCurve8248426Mid)) &&
              Near(identityKey.gradingCurve[kCurveMidInput], kCurve1140733Mid),
          "grading interpolates between the neighbouring keys that set a LUT");
    Check(Near(halfStorm.gradingCurve[kCurveMidInput],
               0.5f * kCurve8286666Mid + 0.5f * IdentityCurve(kCurveMidInput)) &&
              Near(halfStorm.gradingCurve[0], 0.0f) && Near(halfStorm.gradingCurve[kGradingCurveEntries - 1], 1.0f),
          "light params without grading blend toward the identity curve");
    Check(noon.hasGradingCurve && dusk.hasGradingCurve && identityKey.hasGradingCurve && halfStorm.hasGradingCurve,
          "a blend with any graded light params reports a grading curve");
}

void CheckAuthoredFogExtras(const FogData& data)
{
    CheckHyjalNoise(data);
    CheckStormNoiseFollowsTheStormWeight(data);
    CheckClassicGlow(data);
    CheckGlowWithoutClassicFog(data);
    CheckGradingCurves(data);
}
}
