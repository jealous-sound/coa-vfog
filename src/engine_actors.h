#pragma once

#include <cstdint>

constexpr uint32_t kMaxWaterContacts = 32;
constexpr float kWaterContactRange = 48.0f;

struct WaterContact
{
    uint64_t guid = 0;
    float position[3] = {};
    float surface = 0.0f;
    float radius = 0.0f;
    float height = 0.0f;
    bool swimming = false;
    bool onTransport = false;
};

struct WaterContactFrame
{
    WaterContact contacts[kMaxWaterContacts] = {};
    uint32_t count = 0;
    bool truncated = false;
};

float WaterContactDepth(const WaterContact& contact);
float ClientRippleDepthLimit(float height);

namespace engine
{
bool SelectWaterContact(WaterContactFrame& frame, const WaterContact& contact, const float centre[3]);
bool CaptureWaterContactsFrom(uintptr_t objectManager, const float centre[3], WaterContactFrame& out);
bool WaterContactsSupported();
bool CaptureWaterContacts(const float centre[3], WaterContactFrame& out);
}
