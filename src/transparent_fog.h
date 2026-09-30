#pragma once

#include "engine.h"
#include "fog_model.h"

#include <cstdint>

enum class FogPass
{
    WholeFrame,
    BeforeTransparents,
};

struct StockFogFit
{
    bool fogs = false;
    float start = 0.0f;
    float end = 0.0f;
    uint32_t colour = 0;
};

struct M2BatchFogArgs
{
    float start;
    float end;
    float exponent;
    const uint32_t* colour;
};

constexpr float kStockFogFitDepth = 100.0f;
constexpr float kLinearStockFogExponent = 1.0f;
constexpr uint32_t kLightingFogColourAlpha = 0xFF000000u;
constexpr uint32_t kFogColourAlphaMask = 0xFF000000u;
constexpr uint32_t kAdditiveFogColour = 0x00000000u;
constexpr uint32_t kModulateFogColour = 0x00FFFFFFu;
constexpr uint32_t kModulate2xFogColour = 0x00808080u;

struct StockFogFitLight
{
    float exposure;
    float glowToCompensate;
};

StockFogFit FitStockFog(const FogParams& fog, const FrameInputs& in, const StockFogFitLight& light);
bool UsesLightingFogColour(uint32_t colour);
void ApplyStockFogFit(const StockFogFit& fit, M2BatchFogArgs& args);
