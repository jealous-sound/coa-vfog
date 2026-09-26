#include "fog_data.h"

#include "log.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace
{
constexpr char kFileMagic[4] = {'V', 'F', 'D', '1'};
constexpr uint32_t kFormatVersion = 3;
constexpr float kHalfMinutesPerDay = 2880.0f;
constexpr uint32_t kMinimumOutlinePoints = 3;
constexpr uint32_t kClientSelectedLayerFlag = 0x8;

struct Header
{
    char magic[4];
    uint32_t version;
    uint32_t lightCount;
    uint32_t paramsCount;
    uint32_t keyCount;
    uint32_t layerCount;
    uint32_t zoneLightCount;
    uint32_t zonePointCount;
};

template <typename T>
bool ReadArray(std::FILE* f, std::vector<T>& out, uint32_t count)
{
    out.resize(count);
    return count == 0 || std::fread(out.data(), sizeof(T), count, f) == count;
}

bool FileHasSize(std::FILE* file, uint64_t expected)
{
    if (std::fseek(file, 0, SEEK_END) != 0)
        return false;
    const long size = std::ftell(file);
    return size >= 0 && static_cast<uint64_t>(size) == expected &&
           std::fseek(file, sizeof(Header), SEEK_SET) == 0;
}

bool ContainsRange(size_t size, uint32_t first, uint32_t count)
{
    return first <= size && count <= size - first;
}

void UnpackRgb(uint32_t rgb, float* out)
{
    out[0] = static_cast<float>((rgb >> 16) & 0xFF) / 255.0f;
    out[1] = static_cast<float>((rgb >> 8) & 0xFF) / 255.0f;
    out[2] = static_cast<float>(rgb & 0xFF) / 255.0f;
}

void AddScaled(AuthoredLayer& acc, const AuthoredLayer& l, float w)
{
    for (int i = 0; i < 3; ++i)
    {
        acc.diffuse[i] += l.diffuse[i] * w;
        acc.emissive[i] += l.emissive[i] * w;
        acc.shadowEmissive[i] += l.shadowEmissive[i] * w;
    }
    acc.start += l.start * w;
    acc.density += l.density * w;
    acc.shadowMultiplier += l.shadowMultiplier * w;
    acc.upperDensity += l.upperDensity * w;
    acc.upperHeight += l.upperHeight * w;
    acc.lowerDensity += l.lowerDensity * w;
    acc.lowerHeight += l.lowerHeight * w;
    acc.intensity += l.intensity * w;
    acc.g += l.g * w;
    acc.strength += l.strength * w;
    acc.exponent += l.exponent * w;
}

class LayerBlend
{
public:
    void Add(const AuthoredLayer& layer, float weight)
    {
        if (!(layer.flags & kClientSelectedLayerFlag) || layer.density <= 0.0f || weight <= 0.0f)
            return;
        AddScaled(m_sum, layer, weight);
        m_presence += weight;
        if (weight > m_dominantWeight)
        {
            m_dominantWeight = weight;
            m_dominantFlags = layer.flags;
        }
    }

    AuthoredLayer Result() const
    {
        AuthoredLayer out = {};
        if (m_presence <= 0.0f)
            return out;
        AddScaled(out, m_sum, 1.0f / m_presence);
        out.density = m_sum.density;
        out.flags = m_dominantFlags;
        return out;
    }

private:
    AuthoredLayer m_sum = {};
    float m_presence = 0.0f;
    float m_dominantWeight = 0.0f;
    uint32_t m_dominantFlags = 0;
};

class DirectLightBlend
{
public:
    void Add(const float* rgb, float presence, float weight)
    {
        const float w = presence * weight;
        if (w <= 0.0f)
            return;
        for (int i = 0; i < 3; ++i)
            m_sum[i] += rgb[i] * w;
        m_presence += w;
    }

    float Presence() const { return m_presence; }

    void Result(float* rgb) const
    {
        for (int i = 0; i < 3; ++i)
            rgb[i] = m_presence > 0.0f ? m_sum[i] / m_presence : 0.0f;
    }

private:
    float m_sum[3] = {};
    float m_presence = 0.0f;
};

float SquaredDistanceToSegment(float px, float py, float ax, float ay, float bx, float by)
{
    const float ex = bx - ax;
    const float ey = by - ay;
    const float lengthSq = ex * ex + ey * ey;
    const float t = lengthSq > 0.0f ? std::clamp(((px - ax) * ex + (py - ay) * ey) / lengthSq, 0.0f, 1.0f) : 0.0f;
    const float dx = px - (ax + ex * t);
    const float dy = py - (ay + ey * t);
    return dx * dx + dy * dy;
}

bool CrossesRayToPositiveX(float px, float py, float ax, float ay, float bx, float by)
{
    return (ay > py) != (by > py) && px < (bx - ax) * (py - ay) / (by - ay) + ax;
}
}

static_assert(sizeof(Header) == 32, "fogdata header");

void FogData::LightBlend::Add(const Light* light, float weight)
{
    if (weight <= 0.0f)
        return;
    for (int i = 0; i < count; ++i)
        if (lights[i].light == light)
        {
            lights[i].weight += weight;
            return;
        }
    if (count < kMaxBlendedLights)
        lights[count++] = {light, weight};
}

void FogData::LightBlend::Scale(float factor)
{
    for (int i = 0; i < count; ++i)
        lights[i].weight *= factor;
}

bool FogData::Load(const std::string& path)
{
    m_lights.clear();
    m_params.clear();
    m_keys.clear();
    m_layers.clear();
    m_zoneLights.clear();
    m_zonePoints.clear();
    m_zonesLargestFirst.clear();
    m_mapsWithFog.clear();
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f)
    {
        VF_LOG_INFO("no Classic fog data at %s; derived layers only", path.c_str());
        return false;
    }
    Header h = {};
    bool ok = std::fread(&h, sizeof(h), 1, f) == 1 && std::memcmp(h.magic, kFileMagic, sizeof(kFileMagic)) == 0 &&
              h.version == kFormatVersion;
    const uint64_t expectedSize = sizeof(Header) + static_cast<uint64_t>(h.lightCount) * sizeof(Light) +
                                  static_cast<uint64_t>(h.paramsCount) * sizeof(Params) +
                                  static_cast<uint64_t>(h.keyCount) * sizeof(Key) +
                                  static_cast<uint64_t>(h.layerCount) * sizeof(Layer) +
                                  static_cast<uint64_t>(h.zoneLightCount) * sizeof(ZoneLight) +
                                  static_cast<uint64_t>(h.zonePointCount) * sizeof(ZonePoint);
    ok = ok && FileHasSize(f, expectedSize) && ReadArray(f, m_lights, h.lightCount) &&
              ReadArray(f, m_params, h.paramsCount) && ReadArray(f, m_keys, h.keyCount) &&
              ReadArray(f, m_layers, h.layerCount) && ReadArray(f, m_zoneLights, h.zoneLightCount) &&
              ReadArray(f, m_zonePoints, h.zonePointCount);
    std::fclose(f);
    ok = ok && ValidateRecords() && BuildZoneOutlines();
    if (!ok)
    {
        VF_LOG_ERROR("Classic fog data %s is invalid; derived layers only", path.c_str());
        m_lights.clear();
        m_zonesLargestFirst.clear();
        return false;
    }
    CollectMapsWithFog();
    VF_LOG_INFO("Classic fog data: %u lights, %u light params, %u keys, %u layers, %u zone lights", h.lightCount,
                h.paramsCount, h.keyCount, h.layerCount, h.zoneLightCount);
    return true;
}

bool FogData::ValidateRecords() const
{
    for (const Params& params : m_params)
        if (!ContainsRange(m_keys.size(), params.firstKey, params.keyCount))
            return false;
    for (const Key& key : m_keys)
        if (!ContainsRange(m_layers.size(), key.firstLayer, key.layerCount) ||
            key.halfMinuteOfDay >= kHalfMinutesPerDay)
            return false;
    return true;
}

bool FogData::BuildZoneOutlines()
{
    m_zonesLargestFirst.clear();
    for (const ZoneLight& zone : m_zoneLights)
    {
        const Light* light = FindLight(zone.lightId);
        if (!light || zone.pointCount < kMinimumOutlinePoints ||
            !ContainsRange(m_zonePoints.size(), zone.firstPoint, zone.pointCount))
            return false;
        m_zonesLargestFirst.push_back({&zone, light, EnclosedArea(zone)});
    }
    std::stable_sort(m_zonesLargestFirst.begin(), m_zonesLargestFirst.end(),
                     [](const ZoneOutline& a, const ZoneOutline& b) { return a.area > b.area; });
    return true;
}

float FogData::EnclosedArea(const ZoneLight& zone) const
{
    const ZonePoint* points = &m_zonePoints[zone.firstPoint];
    double twiceArea = 0.0;
    for (uint32_t i = 0, j = zone.pointCount - 1; i < zone.pointCount; j = i++)
        twiceArea += static_cast<double>(points[j].x) * points[i].y - static_cast<double>(points[i].x) * points[j].y;
    return static_cast<float>(std::fabs(twiceArea) * 0.5);
}

float FogData::ZoneWeight(const ZoneLight& zone, const float* position) const
{
    if (position[2] < zone.zMin || position[2] > zone.zMax)
        return 0.0f;
    const ZonePoint* points = &m_zonePoints[zone.firstPoint];
    bool inside = false;
    float nearestEdgeSq = std::numeric_limits<float>::max();
    for (uint32_t i = 0, j = zone.pointCount - 1; i < zone.pointCount; j = i++)
    {
        const ZonePoint& a = points[j];
        const ZonePoint& b = points[i];
        if (CrossesRayToPositiveX(position[0], position[1], a.x, a.y, b.x, b.y))
            inside = !inside;
        nearestEdgeSq =
            std::min(nearestEdgeSq, SquaredDistanceToSegment(position[0], position[1], a.x, a.y, b.x, b.y));
    }
    return inside ? std::min(std::sqrt(nearestEdgeSq) / kZoneLightEdgeFade, 1.0f) : 0.0f;
}

bool FogData::IsMapWide(const Light& light)
{
    return light.falloffEnd <= 0.0f && light.position[0] == 0.0f && light.position[1] == 0.0f;
}

float FogData::SphereWeight(const Light& light, const float* position)
{
    const float dx = position[0] - light.position[0];
    const float dy = position[1] - light.position[1];
    const float dz = position[2] - light.position[2];
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (distance <= light.falloffStart)
        return 1.0f;
    if (distance < light.falloffEnd)
        return (light.falloffEnd - distance) / (light.falloffEnd - light.falloffStart);
    return 0.0f;
}

const FogData::Light* FogData::FindLight(uint32_t id) const
{
    for (const Light& light : m_lights)
        if (light.id == id)
            return &light;
    return nullptr;
}

const FogData::Params* FogData::FindParams(uint32_t id) const
{
    auto it = std::lower_bound(m_params.begin(), m_params.end(), id,
                               [](const Params& p, uint32_t v) { return p.id < v; });
    return it != m_params.end() && it->id == id ? &*it : nullptr;
}

FogData::ConditionFog FogData::InterpolateKeys(const Params& params, float halfMinuteOfDay) const
{
    ConditionFog fog = {};
    const uint32_t count = params.keyCount;
    if (count == 0)
        return fog;
    const Key* keys = &m_keys[params.firstKey];

    uint32_t next = 0;
    while (next < count && keys[next].halfMinuteOfDay <= halfMinuteOfDay)
        ++next;
    const uint32_t prev = next == 0 ? count - 1 : next - 1;
    next = next == count ? 0 : next;
    float prevTime = keys[prev].halfMinuteOfDay;
    float nextTime = keys[next].halfMinuteOfDay;
    if (prevTime > halfMinuteOfDay)
        prevTime -= kHalfMinutesPerDay;
    if (nextTime < halfMinuteOfDay || (nextTime == prevTime && count > 1))
        nextTime += kHalfMinutesPerDay;
    const float fraction =
        nextTime > prevTime ? std::clamp((halfMinuteOfDay - prevTime) / (nextTime - prevTime), 0.0f, 1.0f) : 0.0f;

    const Key& a = keys[prev];
    const Key& b = keys[next];
    fog.layerCount = std::min<int>(std::max(a.layerCount, b.layerCount), kMaxAuthoredLayers);
    for (int i = 0; i < fog.layerCount; ++i)
    {
        LayerBlend blend;
        if (i < a.layerCount)
            blend.Add(Unpack(m_layers[a.firstLayer + i]), 1.0f - fraction);
        if (i < b.layerCount)
            blend.Add(Unpack(m_layers[b.firstLayer + i]), fraction);
        fog.layers[i] = blend.Result();
    }
    float directA[3];
    float directB[3];
    UnpackRgb(a.directRgb, directA);
    UnpackRgb(b.directRgb, directB);
    for (int i = 0; i < 3; ++i)
        fog.directLight[i] = directA[i] + (directB[i] - directA[i]) * fraction;
    fog.directLightPresence = 1.0f;
    return fog;
}

FogData::ConditionFog FogData::BlendConditions(const ConditionFog& a, const ConditionFog& b, float bWeight)
{
    ConditionFog blended = {};
    blended.layerCount = std::max(a.layerCount, b.layerCount);
    for (int i = 0; i < blended.layerCount; ++i)
    {
        LayerBlend blend;
        blend.Add(a.layers[i], 1.0f - bWeight);
        blend.Add(b.layers[i], bWeight);
        blended.layers[i] = blend.Result();
    }
    DirectLightBlend direct;
    direct.Add(a.directLight, a.directLightPresence, 1.0f - bWeight);
    direct.Add(b.directLight, b.directLightPresence, bWeight);
    direct.Result(blended.directLight);
    blended.directLightPresence = direct.Presence();
    return blended;
}

AuthoredLayer FogData::Unpack(const Layer& layer)
{
    AuthoredLayer out = {};
    UnpackRgb(layer.diffuseRgb, out.diffuse);
    UnpackRgb(layer.emissiveRgb, out.emissive);
    UnpackRgb(layer.shadowEmissiveRgb, out.shadowEmissive);
    out.start = layer.start;
    out.density = layer.density;
    out.shadowMultiplier = layer.shadowMultiplier;
    out.upperDensity = layer.upperDensity;
    out.upperHeight = layer.upperHeight;
    out.lowerDensity = layer.lowerDensity;
    out.lowerHeight = layer.lowerHeight;
    out.intensity = layer.intensity;
    out.g = layer.g;
    out.strength = layer.strength;
    out.exponent = layer.exponent;
    out.flags = layer.flags;
    return out;
}

FogData::ConditionFog FogData::LightConditionFog(const Light& light, float halfMinuteOfDay,
                                                 const LightParamsSelection& selection) const
{
    const int effectSlot = selection.screenEffectSlot;
    if (effectSlot >= 0 && effectSlot < kLightParamsSlots && light.paramsBySlot[effectSlot] != 0)
    {
        const Params* effect = FindParams(light.paramsBySlot[effectSlot]);
        return effect ? InterpolateKeys(*effect, halfMinuteOfDay) : ConditionFog{};
    }
    const Params* clear = FindParams(light.paramsBySlot[kClearSlot]);
    const ConditionFog clearFog = clear ? InterpolateKeys(*clear, halfMinuteOfDay) : ConditionFog{};
    const float storm = std::clamp(selection.stormBlend, 0.0f, 1.0f);
    if (storm <= 0.0f || light.paramsBySlot[kStormSlot] == 0)
        return clearFog;
    const Params* stormParams = FindParams(light.paramsBySlot[kStormSlot]);
    const ConditionFog stormFog = stormParams ? InterpolateKeys(*stormParams, halfMinuteOfDay) : ConditionFog{};
    return BlendConditions(clearFog, stormFog, storm);
}

FogData::LightBlend FogData::BlendLights(int mapId, const float* position) const
{
    const auto byWeight = [](const Contribution& a, const Contribution& b) { return a.weight < b.weight; };
    Contribution spheres[kMaxSphereLights] = {};
    int sphereCount = 0;
    const Light* mapWide = nullptr;
    for (const Light& light : m_lights)
    {
        if (light.mapId != mapId)
            continue;
        if (IsMapWide(light))
        {
            if (!mapWide)
                mapWide = &light;
            continue;
        }
        const float weight = SphereWeight(light, position);
        if (weight <= 0.0f)
            continue;
        if (sphereCount < kMaxSphereLights)
            spheres[sphereCount++] = {&light, weight};
        else
        {
            auto weakest = std::min_element(spheres, spheres + sphereCount, byWeight);
            if (weakest->weight < weight)
                *weakest = {&light, weight};
        }
    }
    float sphereWeight = 0.0f;
    for (int i = 0; i < sphereCount; ++i)
        sphereWeight += spheres[i].weight;
    const float sphereNormalization = sphereWeight > 1.0f ? 1.0f / sphereWeight : 1.0f;

    LightBlend background = {};
    if (mapWide)
        background.Add(mapWide, 1.0f);
    int nesting = 0;
    for (const ZoneOutline& outline : m_zonesLargestFirst)
    {
        if (outline.zone->mapId != mapId || nesting == kMaxZoneLightNesting)
            continue;
        const float weight = ZoneWeight(*outline.zone, position);
        if (weight <= 0.0f)
            continue;
        background.Scale(1.0f - weight);
        background.Add(outline.light, weight);
        ++nesting;
    }
    background.Scale(std::max(1.0f - sphereWeight * sphereNormalization, 0.0f));

    LightBlend blend = {};
    for (int i = 0; i < sphereCount; ++i)
        blend.Add(spheres[i].light, spheres[i].weight * sphereNormalization);
    for (int i = 0; i < background.count; ++i)
        blend.Add(background.lights[i].light, background.lights[i].weight);
    return blend;
}

bool FogData::Resolve(int mapId, const float* position, float dayFraction, const LightParamsSelection& selection,
                      AuthoredFog& out) const
{
    std::memset(&out, 0, sizeof(out));
    if (m_lights.empty() || mapId < 0)
        return false;

    if (!std::binary_search(m_mapsWithFog.begin(), m_mapsWithFog.end(), mapId))
        return false;

    const LightBlend blend = BlendLights(mapId, position);
    float classicWeight = 0.0f;
    for (int i = 0; i < blend.count; ++i)
        classicWeight += blend.lights[i].weight;
    if (classicWeight < kMinimumClassicCoverage)
        return false;

    const float halfMinuteOfDay = std::fmod(std::max(dayFraction, 0.0f), 1.0f) * kHalfMinutesPerDay;
    LayerBlend layerBlends[kMaxAuthoredLayers];
    DirectLightBlend directLight;
    for (int i = 0; i < blend.count; ++i)
    {
        const Light& light = *blend.lights[i].light;
        const float weight = blend.lights[i].weight / classicWeight;
        const ConditionFog fog = LightConditionFog(light, halfMinuteOfDay, selection);
        for (int j = 0; j < fog.layerCount; ++j)
            layerBlends[j].Add(fog.layers[j], weight);
        directLight.Add(fog.directLight, fog.directLightPresence, weight);
        out.layerCount = std::max(out.layerCount, fog.layerCount);
        out.lightIds[out.lightCount] = light.id;
        out.lightWeights[out.lightCount] = weight;
        ++out.lightCount;
    }
    for (int j = 0; j < out.layerCount; ++j)
        out.layers[j] = layerBlends[j].Result();
    out.hasClassicDirectLight = directLight.Presence() > 0.0f;
    directLight.Result(out.classicDirectLight);
    out.coverage = classicWeight;
    return true;
}

bool FogData::HasFogInAnySlot(const Light& light) const
{
    for (uint32_t params : light.paramsBySlot)
        if (params != 0 && FindParams(params))
            return true;
    return false;
}

void FogData::CollectMapsWithFog()
{
    m_mapsWithFog.clear();
    for (const Light& light : m_lights)
        if (HasFogInAnySlot(light))
            m_mapsWithFog.push_back(light.mapId);
    std::sort(m_mapsWithFog.begin(), m_mapsWithFog.end());
    m_mapsWithFog.erase(std::unique(m_mapsWithFog.begin(), m_mapsWithFog.end()), m_mapsWithFog.end());
}

FogData& GlobalFogData()
{
    static FogData data;
    return data;
}
