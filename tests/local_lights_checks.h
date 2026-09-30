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
        engine::SelectLocalPointLight(forwards, PointLightFixture(static_cast<float>(i)), camera, {});
        engine::SelectLocalPointLight(backwards, PointLightFixture(static_cast<float>(15 - i)), camera, {});
    }
    bool strongest = forwards.pointLightCount == kMaxLocalPointLights &&
                     backwards.pointLightCount == kMaxLocalPointLights;
    for (uint32_t i = 0; i < kMaxLocalPointLights; ++i)
        strongest = strongest && forwards.pointLights[i].position[0] == static_cast<float>(i) &&
                    backwards.pointLights[i].position[0] == static_cast<float>(i);
    Check(strongest, "eight strongest nearby point lights are selected independently of native list order");
    light = PointLightFixture(40.0f);
    light.color[0] = 64.0f;
    const bool brighterSelected = engine::SelectLocalPointLight(forwards, light, camera, {});
    bool present = false;
    for (uint32_t i = 0; i < forwards.pointLightCount; ++i)
        present = present || forwards.pointLights[i].position[0] == 40.0f;
    Check(brighterSelected && present && forwards.pointLightCount == kMaxLocalPointLights,
          "point-light selection accounts for brightness as well as distance");
    light = PointLightFixture(1000.0f);
    const bool farSelected = engine::SelectLocalPointLight(forwards, light, camera, {});
    light.position[0] = InvalidLocalLightFloat();
    const bool invalidSelected = engine::SelectLocalPointLight(forwards, light, camera, {});
    Check(!farSelected && !invalidSelected && forwards.pointLightCount == kMaxLocalPointLights,
          "point-light selection rejects invalid positions and centres outside the capture neighbourhood");

    bool clearedBoth = true;
    for (bool withPointLights : {true, false})
    {
        LocalLightInputs unsupported = forwards;
        unsupported.cameraInterior = true;
        unsupported.interiorBlend = 1.0f;
        const bool captured = engine::CaptureLocalLightInputs(camera, withPointLights, {}, unsupported);
        clearedBoth = clearedBoth && !captured && unsupported.pointLightCount == 0 &&
                      !unsupported.cameraInterior && unsupported.interiorBlend == 0.0f &&
                      unsupported.capture == LocalLightCapture::UnsupportedClient;
    }
    Check(clearedBoth,
          "native local-light acquisition rejects the harness image as an unsupported client and clears stale inputs "
          "whether or not the point-light walk is requested (the walk skip itself needs the client image)");
}

struct TablePointLight
{
    uintptr_t scene = 0;
    uint32_t frameStamp = 0;
    uint32_t type = 1;
    float position[3] = {};
    float viewPosition[3] = {};
    float direction[3] = {};
    float ambient[3] = {};
    float color[3] = {1.0f, 0.5f, 0.25f};
    float specular[3] = {};
    float attenuation[3] = {0.0f, 0.7f, 0.03f};
    uint32_t enabled = 1;
    uintptr_t previousLink = 0;
    uintptr_t next = 0;
};

static_assert(sizeof(TablePointLight) == 0x6C && offsetof(TablePointLight, color) == 0x3C &&
                  offsetof(TablePointLight, enabled) == 0x60 && offsetof(TablePointLight, next) == 0x68,
              "the table fixture follows the client's point-light layout recorded in README");

constexpr size_t kSceneBucketsWord = 0x24 / sizeof(uintptr_t);
constexpr size_t kTableBucketCount = 4096;
constexpr size_t kLitBucket = 1234;

struct PointLightTable
{
    uintptr_t sceneSlot = 0;
    uintptr_t scene[kSceneBucketsWord + 1] = {};
    uintptr_t buckets[kTableBucketCount] = {};
    TablePointLight lights[2];
};

void LinkPointLightTable(PointLightTable& table)
{
    table.sceneSlot = reinterpret_cast<uintptr_t>(table.scene);
    table.scene[kSceneBucketsWord] = reinterpret_cast<uintptr_t>(table.buckets);
    uintptr_t* link = &table.buckets[kLitBucket];
    float x = 10.0f;
    for (TablePointLight& light : table.lights)
    {
        light = {};
        light.position[0] = x;
        x += 10.0f;
        light.scene = table.sceneSlot;
        light.previousLink = reinterpret_cast<uintptr_t>(link);
        *link = reinterpret_cast<uintptr_t>(&light);
        link = &light.next;
    }
}

LocalLightCapture CaptureTable(PointLightTable& table, LocalLightInputs& out)
{
    const float camera[3] = {};
    return engine::CapturePointLightTable(reinterpret_cast<uintptr_t>(&table.sceneSlot), camera, {}, out);
}

bool RejectedWith(PointLightTable& table, LocalLightCapture expected)
{
    LocalLightInputs out;
    out.pointLightCount = 1;
    return CaptureTable(table, out) == expected && out.capture == expected && out.pointLightCount == 0;
}

void CheckPointLightTableCapture()
{
    auto table = std::make_unique<PointLightTable>();
    LinkPointLightTable(*table);
    LocalLightInputs walked;
    const LocalLightCapture captured = CaptureTable(*table, walked);
    Check(captured == LocalLightCapture::Captured && walked.capture == LocalLightCapture::Captured &&
              walked.pointLightCount == 2 &&
              walked.pointLights[0].nativeId == reinterpret_cast<uintptr_t>(&table->lights[0]) &&
              walked.pointLights[1].nativeId == reinterpret_cast<uintptr_t>(&table->lights[1]) &&
              walked.pointLights[1].enabled == 1 && walked.pointLights[1].attenuation[2] == 0.03f,
          "the point-light walk captures every enabled point light of a linked table with its address and inputs");

    table->lights[1].enabled = 0;
    const bool disabled = RejectedWith(*table, LocalLightCapture::DisabledLight);
    LinkPointLightTable(*table);
    table->lights[1].type = 0;
    const bool nonPoint = RejectedWith(*table, LocalLightCapture::NonPointLight);
    LinkPointLightTable(*table);
    table->lights[1].previousLink = reinterpret_cast<uintptr_t>(&table->buckets[kLitBucket]);
    const bool damaged = RejectedWith(*table, LocalLightCapture::DamagedTable);
    Check(disabled && nonPoint && damaged,
          "the point-light walk names why it rejects a table: a disabled light, a non-point light or a broken link, "
          "and keeps no lights from it");

    void* unmapped = VirtualAlloc(nullptr, 65536, MEM_RESERVE, PAGE_NOACCESS);
    LinkPointLightTable(*table);
    table->sceneSlot = reinterpret_cast<uintptr_t>(unmapped);
    const bool fault = unmapped && RejectedWith(*table, LocalLightCapture::ReadFault);
    if (unmapped)
        VirtualFree(unmapped, 0, MEM_RELEASE);
    table->sceneSlot = 0;
    LocalLightInputs noScene;
    const bool empty = CaptureTable(*table, noScene) == LocalLightCapture::Captured && noScene.pointLightCount == 0;
    Check(fault && empty,
          "a scene pointer into unmapped memory is a read fault, and no scene is an empty capture, not a rejection");
}

constexpr float kStatueGreen = 255.0f;
constexpr float kStatueRed = 65.9f;
constexpr float kLinearGamma = 2.2f;

LocalPointLight ColouredLightFixture(float x, float red, float green, float blue)
{
    LocalPointLight light = PointLightFixture(x);
    light.color[0] = red;
    light.color[1] = green;
    light.color[2] = blue;
    return light;
}

bool UploadedColourIs(const LocalPointLight& light, const PointLightUpload& upload, const float (&expected)[3])
{
    float uploaded[3];
    engine::UploadedPointLightColor(light.color, upload, uploaded);
    bool matches = true;
    for (int channel = 0; channel < 3; ++channel)
        matches = matches && LocalNear(uploaded[channel], expected[channel]);
    if (!matches)
        std::printf("     uploaded colour %.6g %.6g %.6g, expected %.6g %.6g %.6g\n", uploaded[0], uploaded[1],
                    uploaded[2], expected[0], expected[1], expected[2]);
    return matches;
}

void CheckHdrLocalLightColours()
{
    const PointLightUpload linear = {true, 1.0f};
    const PointLightUpload gamma = {false, 1.0f};
    const LocalPointLight statue = ColouredLightFixture(0.0f, kStatueRed, kStatueGreen, kStatueGreen);
    const float statueChroma = std::pow(kStatueRed / kStatueGreen, kLinearGamma) * kStatueGreen;
    Check(UploadedColourIs(statue, linear, {statueChroma, kStatueGreen, kStatueGreen}),
          "an HDR point light linearises its chromaticity and keeps its peak intensity linear (255, not 255^2.2)");
    const LocalPointLight halfStatue =
        ColouredLightFixture(0.0f, kStatueRed * 0.5f, kStatueGreen * 0.5f, kStatueGreen * 0.5f);
    Check(UploadedColourIs(halfStatue, linear, {statueChroma * 0.5f, kStatueGreen * 0.5f, kStatueGreen * 0.5f}),
          "halving an HDR light's intensity halves its uploaded colour");
    const LocalPointLight torch = ColouredLightFixture(0.0f, 1.0f, 0.5f, 0.25f);
    Check(UploadedColourIs(torch, linear, {1.0f, std::pow(0.5f, kLinearGamma), std::pow(0.25f, kLinearGamma)}) &&
              UploadedColourIs(torch, {true, 2.0f},
                               {2.0f, 2.0f * std::pow(0.5f, kLinearGamma), 2.0f * std::pow(0.25f, kLinearGamma)}),
          "a point light no brighter than 1 keeps the gamma 2.2 decode, times LocalLightIntensity");
    Check(UploadedColourIs(statue, gamma, {kStatueRed, kStatueGreen, kStatueGreen}),
          "with ColorSpace=0 the captured colour is uploaded unchanged");

    const float camera[3] = {};
    const LocalPointLight dim = ColouredLightFixture(0.0f, 0.5f, 0.5f, 0.5f);
    LocalLightInputs linearDim;
    LocalLightInputs gammaDim;
    engine::SelectLocalPointLight(linearDim, dim, camera, linear);
    engine::SelectLocalPointLight(gammaDim, dim, camera, gamma);
    const float linearPeak = std::pow(0.5f, kLinearGamma);
    Check(linearDim.pointLightCount == 1 && gammaDim.pointLightCount == 1 &&
              LocalNear(linearDim.pointLights[0].cutoff,
                        std::sqrt(linearPeak / kLocalPointLightContributionCutoff - 1.0f)) &&
              LocalNear(gammaDim.pointLights[0].cutoff, std::sqrt(0.5f / kLocalPointLightContributionCutoff - 1.0f)) &&
              LocalNear(linearDim.pointLights[0].uploadedColor[0], linearPeak),
          "a point light's reach is cut off at 1/256 of the colour it uploads, not of the captured gamma colour");

    const LocalPointLight nearGrey = ColouredLightFixture(0.0f, 0.9f, 0.9f, 0.9f);
    const LocalPointLight fartherBright = ColouredLightFixture(1.163f, 2.0f, 2.0f, 2.0f);
    LocalLightInputs linearOrder;
    LocalLightInputs gammaOrder;
    for (const LocalPointLight* light : {&nearGrey, &fartherBright})
    {
        engine::SelectLocalPointLight(linearOrder, *light, camera, linear);
        engine::SelectLocalPointLight(gammaOrder, *light, camera, gamma);
    }
    Check(linearOrder.pointLightCount == 2 && gammaOrder.pointLightCount == 2 &&
              linearOrder.pointLights[0].position[0] == fartherBright.position[0] &&
              gammaOrder.pointLights[0].position[0] == nearGrey.position[0],
          "point lights are ranked by the colour they upload: in linear light a farther HDR light outranks a nearer "
          "grey one that is brighter in gamma");

    LocalLightInputs unlit;
    Check(!engine::SelectLocalPointLight(unlit, torch, camera, {true, 0.0f}) && unlit.pointLightCount == 0,
          "LocalLightIntensity=0 uploads no point light");
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
    edited.localLightPhase = 2.0f;
    edited.interiorDensity = -1.0f;
    edited.localLights = false;
    edited.interiorAware = false;
    settings.Apply(edited);
    Check(settings.Get().localLightIntensity == 8.0f && settings.Get().localLightPhase == 0.9f &&
              settings.Get().interiorDensity == 0.0f && !settings.Get().localLights &&
              !settings.Get().interiorAware && settings.HasUnsavedChanges(),
          "local-light and interior controls clamp and participate in live-setting changes");
    edited.localLightPhase = -2.0f;
    settings.Apply(edited);
    Config shipped;
    Config phased = shipped;
    phased.localLightPhase = 0.5f;
    Check(settings.Get().localLightPhase == -0.9f && shipped.localLightPhase == 0.3f &&
              !SameFogSettings(shipped, phased) &&
              SettingChanges(shipped, phased) == "LocalLightPhase 0.3 -> 0.5",
          "LocalLightPhase defaults to 0.3, clamps to -0.9..0.9, discards fog history and logs its changes");
}
