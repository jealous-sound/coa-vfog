#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct AuthoredLayer
{
    float diffuse[3];
    float emissive[3];
    float shadowEmissive[3];
    float start;
    float density;
    float shadowMultiplier;
    float upperDensity;
    float upperHeight;
    float lowerDensity;
    float lowerHeight;
    float intensity;
    float g;
    float strength;
    float exponent;
    uint32_t flags;
};

constexpr int kMaxAuthoredLayers = 3;
constexpr float kMinimumClassicCoverage = 0.5f;
constexpr int kMaxSphereLights = 3;
constexpr int kMaxZoneLightNesting = 3;
constexpr int kMaxBlendedLights = kMaxSphereLights + kMaxZoneLightNesting + 1;
constexpr int kNoScreenEffectLightSlot = -1;
constexpr float kZoneLightEdgeFade = 100.0f;

struct LightParamsSelection
{
    float stormBlend = 0.0f;
    int screenEffectSlot = kNoScreenEffectLightSlot;
};

struct AuthoredFog
{
    int layerCount;
    AuthoredLayer layers[kMaxAuthoredLayers];
    int lightCount;
    uint32_t lightIds[kMaxBlendedLights];
    float lightWeights[kMaxBlendedLights];
    float coverage;
    bool hasClassicDirectLight;
    float classicDirectLight[3];
};

class FogData
{
public:
    bool Load(const std::string& path);
    bool Loaded() const { return !m_lights.empty(); }

    bool Resolve(int mapId, const float* position, float dayFraction, const LightParamsSelection& selection,
                 AuthoredFog& out) const;

    static constexpr int kLightParamsSlots = 8;
    static constexpr int kClearSlot = 0;
    static constexpr int kStormSlot = 2;
    static constexpr int kDeathSlot = 4;

private:
    struct Light
    {
        uint32_t id;
        int32_t mapId;
        float position[3];
        float falloffStart;
        float falloffEnd;
        uint32_t paramsBySlot[kLightParamsSlots];
    };
    struct Params
    {
        uint32_t id;
        uint32_t firstKey;
        uint32_t keyCount;
    };
    struct Key
    {
        uint16_t halfMinuteOfDay;
        uint16_t layerCount;
        uint32_t firstLayer;
        uint32_t directRgb;
    };
    struct Layer
    {
        uint32_t diffuseRgb;
        uint32_t emissiveRgb;
        uint32_t shadowEmissiveRgb;
        uint32_t flags;
        float start;
        float density;
        float shadowMultiplier;
        float upperDensity;
        float upperHeight;
        float lowerDensity;
        float lowerHeight;
        float intensity;
        float g;
        float strength;
        float exponent;
    };
    struct ZoneLight
    {
        uint32_t id;
        int32_t mapId;
        uint32_t lightId;
        float zMin;
        float zMax;
        uint32_t firstPoint;
        uint32_t pointCount;
    };
    struct ZonePoint
    {
        float x;
        float y;
    };
    static_assert(sizeof(Light) == 60 && sizeof(Params) == 12 && sizeof(Key) == 12 && sizeof(Layer) == 60 &&
                      sizeof(ZoneLight) == 28 && sizeof(ZonePoint) == 8,
                  "records match the struct formats of tools/convert_classic_fog.py");

    struct ZoneOutline
    {
        const ZoneLight* zone;
        const Light* light;
        float area;
    };
    struct Contribution
    {
        const Light* light;
        float weight;
    };
    struct LightBlend
    {
        Contribution lights[kMaxBlendedLights];
        int count;
        void Add(const Light* light, float weight);
        void Scale(float factor);
    };
    struct ConditionFog
    {
        AuthoredLayer layers[kMaxAuthoredLayers];
        int layerCount;
        float directLight[3];
        float directLightPresence;
    };

    static bool IsMapWide(const Light& light);
    static float SphereWeight(const Light& light, const float* position);
    const Light* FindLight(uint32_t id) const;
    const Params* FindParams(uint32_t id) const;
    float EnclosedArea(const ZoneLight& zone) const;
    float ZoneWeight(const ZoneLight& zone, const float* position) const;
    bool ValidateRecords() const;
    bool BuildZoneOutlines();
    bool HasFogInAnySlot(const Light& light) const;
    void CollectMapsWithFog();
    LightBlend BlendLights(int mapId, const float* position) const;
    static AuthoredLayer Unpack(const Layer& layer);
    static ConditionFog BlendConditions(const ConditionFog& a, const ConditionFog& b, float bWeight);
    ConditionFog InterpolateKeys(const Params& params, float halfMinuteOfDay) const;
    ConditionFog LightConditionFog(const Light& light, float halfMinuteOfDay,
                                   const LightParamsSelection& selection) const;

    std::vector<Light> m_lights;
    std::vector<Params> m_params;
    std::vector<Key> m_keys;
    std::vector<Layer> m_layers;
    std::vector<ZoneLight> m_zoneLights;
    std::vector<ZonePoint> m_zonePoints;
    std::vector<ZoneOutline> m_zonesLargestFirst;
    std::vector<int32_t> m_mapsWithFog;
};

FogData& GlobalFogData();
