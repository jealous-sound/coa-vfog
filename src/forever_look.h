#pragma once

#include "engine.h"
#include "fog_data.h"

#include <d3d9.h>

#include <cstdint>

class FogDevice;

struct ForeverLook
{
    bool valid = false;
    float coverage = 0.0f;
    bool hasGlow = false;
    float glow = 0.0f;
    float gradingCurve[kGradingCurveEntries] = {};
};

struct ForeverLookFrame
{
    engine::ScreenEffects effects;
    bool cameraInLiquid = false;
    bool hasWorldViewport = false;
    D3DVIEWPORT9 worldViewport = {};
    ForeverLook look;
};

struct ForeverLookStatus
{
    bool glowDelivered = false;
    uint8_t clientGlowByte = 0;
    uint8_t deliveredGlowByte = 0;
    float weight = 0.0f;
    bool graded = false;
    float gradingStrength = 0.0f;
    const char* grading = "";
};

float ForeverLookWeight(float coverage);
uint8_t ForeverGlowByte(uint8_t clientByte, float foreverGlow, float weight);

void EnableForeverLook(FogDevice* (*device)(), bool glowPassColour, bool gradingPlacement);
void UseTestForeverLookFrame(const ForeverLookFrame& frame);
void ForeverLookAtWorldDone();
void ForeverLookAtFrameEnd();
bool DeliveredGlowThisFrame(float& amount);
ForeverLookStatus LastForeverLookStatus();
