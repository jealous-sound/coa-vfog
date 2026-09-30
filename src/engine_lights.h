#pragma once

#include <cstdint>

constexpr uint32_t kMaxLocalPointLights = 8;
constexpr float kLocalPointLightContributionCutoff = 1.0f / 256.0f;
constexpr float kMaxLocalPointLightRadius = 200.0f;
constexpr float kLocalLightGamma = 2.2f;

struct PointLightUpload
{
    bool linear = true;
    float intensity = 1.0f;
};

struct LocalPointLight
{
    float position[3] = {};
    float color[3] = {};
    float attenuation[3] = {};
    float uploadedColor[3] = {};
    float cutoff = 0.0f;
};

struct LocalLightInputs
{
    LocalPointLight pointLights[kMaxLocalPointLights] = {};
    uint32_t pointLightCount = 0;
    float interiorBlend = 0.0f;
    bool cameraInterior = false;
};

namespace engine
{
void UploadedPointLightColor(const float captured[3], const PointLightUpload& upload, float uploaded[3]);
float PointLightCutoff(const float color[3], const float attenuation[3]);
bool SelectLocalPointLight(LocalLightInputs& out, const LocalPointLight& light, const float cameraPosition[3],
                           const PointLightUpload& upload);
bool CaptureLocalLightInputs(const float cameraPosition[3], bool withPointLights, const PointLightUpload& upload,
                             LocalLightInputs& out);
}
