#pragma once

float InvalidLocalLightFloat()
{
    const uint32_t bits = 0x7FC00000;
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

bool LocalNear(float a, float b)
{
    return std::fabs(a - b) <= 1.0e-5f * std::max(1.0f, std::max(std::fabs(a), std::fabs(b)));
}

LocalPointLight PointLightFixture(float x)
{
    LocalPointLight light;
    light.position[0] = x;
    light.color[0] = 1.0f;
    light.color[1] = 0.5f;
    light.color[2] = 0.25f;
    light.attenuation[0] = 1.0f;
    light.attenuation[2] = 1.0f;
    return light;
}

void CheckLocalLightInputs()
{
    LocalPointLight light = PointLightFixture(0.0f);
    const float quadraticRadius = engine::PointLightCutoff(light.color, light.attenuation);
    light.attenuation[1] = 2.0f;
    light.attenuation[2] = 0.0f;
    const float linearRadius = engine::PointLightCutoff(light.color, light.attenuation);
    light.attenuation[1] = 0.0f;
    const float constantRadius = engine::PointLightCutoff(light.color, light.attenuation);
    Check(LocalNear(quadraticRadius, std::sqrt(255.0f)) && LocalNear(linearRadius, 127.5f) &&
              constantRadius == kMaxLocalPointLightRadius,
          "point-light bounds follow native quadratic, linear and constant attenuation");
    light.attenuation[0] = 1000.0f;
    const float dimRadius = engine::PointLightCutoff(light.color, light.attenuation);
    light.attenuation[0] = -1.0f;
    const float negativeRadius = engine::PointLightCutoff(light.color, light.attenuation);
    light.attenuation[0] = 1.0f;
    light.color[1] = InvalidLocalLightFloat();
    const float invalidRadius = engine::PointLightCutoff(light.color, light.attenuation);
    Check(dimRadius == 0.0f && negativeRadius == 0.0f && invalidRadius == 0.0f,
          "dim, negative and nonfinite point-light inputs cannot create a fog light");

    const float camera[3] = {};
    LocalLightInputs forwards;
    LocalLightInputs backwards;
    for (int i = 0; i < 16; ++i)
    {
        engine::SelectLocalPointLight(forwards, PointLightFixture(static_cast<float>(i)), camera);
        engine::SelectLocalPointLight(backwards, PointLightFixture(static_cast<float>(15 - i)), camera);
    }
    bool strongest = forwards.pointLightCount == kMaxLocalPointLights &&
                     backwards.pointLightCount == kMaxLocalPointLights;
    for (uint32_t i = 0; i < kMaxLocalPointLights; ++i)
        strongest = strongest && forwards.pointLights[i].position[0] == static_cast<float>(i) &&
                    backwards.pointLights[i].position[0] == static_cast<float>(i);
    Check(strongest, "eight strongest nearby point lights are selected independently of native list order");
    light = PointLightFixture(40.0f);
    light.color[0] = 64.0f;
    const bool brighterSelected = engine::SelectLocalPointLight(forwards, light, camera);
    bool present = false;
    for (uint32_t i = 0; i < forwards.pointLightCount; ++i)
        present = present || forwards.pointLights[i].position[0] == 40.0f;
    Check(brighterSelected && present && forwards.pointLightCount == kMaxLocalPointLights,
          "point-light selection accounts for brightness as well as distance");
    light = PointLightFixture(1000.0f);
    const bool farSelected = engine::SelectLocalPointLight(forwards, light, camera);
    light.position[0] = InvalidLocalLightFloat();
    const bool invalidSelected = engine::SelectLocalPointLight(forwards, light, camera);
    Check(!farSelected && !invalidSelected && forwards.pointLightCount == kMaxLocalPointLights,
          "point-light selection rejects invalid positions and centres outside the capture neighbourhood");

    LocalLightInputs unsupported = forwards;
    unsupported.cameraInterior = true;
    unsupported.interiorBlend = 1.0f;
    const bool captured = engine::CaptureLocalLightInputs(camera, unsupported);
    Check(!captured && unsupported.pointLightCount == 0 && !unsupported.cameraInterior &&
              unsupported.interiorBlend == 0.0f,
          "native local-light acquisition rejects the harness image and clears stale inputs");
}

FrameInputs InteriorFogFixture()
{
    FrameInputs input = {};
    input.farClip = 1000.0f;
    input.fogStart = 60.0f;
    input.fogEnd = 600.0f;
    input.zoneFogDistance = 600.0f;
    input.toLight[2] = 1.0f;
    input.fogColor = 0xFF4080C0;
    input.directColor = 0xFFFFFFFF;
    input.sunColor = 0xFFFFFFFF;
    return input;
}

void CheckInteriorFogInputs()
{
    Config config;
    FrameInputs input = InteriorFogFixture();
    const FogParams outside = BuildFogParams(input, config, nullptr);
    input.localLights.interiorBlend = 1.0f;
    const FogParams unclassified = BuildFogParams(input, config, nullptr);
    input.localLights.cameraInterior = true;
    input.localLights.interiorBlend = 0.0f;
    const FogParams threshold = BuildFogParams(input, config, nullptr);
    Check(LocalNear(outside.layers[0].density, unclassified.layers[0].density) &&
              LocalNear(outside.layers[0].density, threshold.layers[0].density) &&
              LocalNear(outside.layers[0].diffuse[0], unclassified.layers[0].diffuse[0]),
          "outdoor fog is unchanged without interior membership or at the doorway threshold");

    input.localLights.interiorBlend = 0.5f;
    const FogParams middle = BuildFogParams(input, config, nullptr);
    input.localLights.interiorBlend = 1.0f;
    const FogParams inside = BuildFogParams(input, config, nullptr);
    bool fades = true;
    for (int i = 0; i < kSceneLayers; ++i)
        fades = fades && LocalNear(middle.layers[i].density, outside.layers[i].density * 0.575f) &&
                LocalNear(inside.layers[i].density, outside.layers[i].density * 0.15f);
    for (int i = 0; i < kFogLayers; ++i)
        for (int channel = 0; channel < 3; ++channel)
            fades = fades && LocalNear(middle.layers[i].diffuse[channel], outside.layers[i].diffuse[channel] * 0.5f) &&
                    inside.layers[i].diffuse[channel] == 0.0f;
    Check(fades && inside.lightVisibility == 0.0f && LocalNear(middle.lightVisibility, 0.5f),
          "interior transitions continuously reduce outdoor density and direct sunlight");
    Check(LocalNear(inside.layers[kDistanceFogLayer].density, outside.layers[kDistanceFogLayer].density) &&
              LocalNear(inside.layers[kDistanceFogLayer].emissive[2], outside.layers[kDistanceFogLayer].emissive[2]) &&
              inside.farLimit == input.fogEnd,
          "interior transitions preserve the native WMO distance-fog colour and range");

    config.interiorAware = false;
    const FogParams disabled = BuildFogParams(input, config, nullptr);
    config.interiorAware = true;
    input.localLights.interiorBlend = InvalidLocalLightFloat();
    const FogParams invalid = BuildFogParams(input, config, nullptr);
    Check(LocalNear(disabled.layers[0].density, outside.layers[0].density) &&
              LocalNear(disabled.layers[0].diffuse[0], outside.layers[0].diffuse[0]) &&
              LocalNear(invalid.layers[0].density, outside.layers[0].density),
          "interior bypass and nonfinite transition weights preserve outdoor fog");

    AuthoredFog authored = {};
    authored.layerCount = 1;
    authored.coverage = 1.0f;
    authored.layers[0].density = 1.0f;
    authored.layers[0].exponent = 1.0f;
    authored.layers[0].intensity = 1.0f;
    authored.layers[0].diffuse[0] = 1.0f;
    authored.layers[0].emissive[0] = 0.5f;
    input.localLights.interiorBlend = 0.0f;
    const FogParams authoredOutside = BuildFogParams(input, config, &authored);
    input.localLights.interiorBlend = 1.0f;
    const FogParams authoredInside = BuildFogParams(input, config, &authored);
    Check(LocalNear(authoredInside.layers[0].density, authoredOutside.layers[0].density * 0.15f) &&
              authoredInside.layers[0].diffuse[0] == 0.0f &&
              authoredOutside.layers[kDistanceFogLayer].density == 0.0f &&
              LocalNear(authoredInside.layers[kDistanceFogLayer].density, inside.layers[kDistanceFogLayer].density),
          "dense Classic outdoor layers give way to native interior distance fog");

    ConfigStore settings;
    Config edited = settings.Get();
    edited.localLightIntensity = 20.0f;
    edited.interiorDensity = -1.0f;
    edited.localLights = false;
    edited.interiorAware = false;
    settings.Apply(edited);
    Check(settings.Get().localLightIntensity == 8.0f && settings.Get().interiorDensity == 0.0f &&
              !settings.Get().localLights && !settings.Get().interiorAware && settings.HasUnsavedChanges(),
          "local-light and interior controls clamp and participate in live-setting changes");
}
