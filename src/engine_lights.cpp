#include "engine_lights.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>

namespace
{
constexpr uintptr_t kClientImageBase = 0x00400000;
constexpr uint32_t kClientTimestamp = 0x4C2452FE;
constexpr uintptr_t kWorldM2Scene = 0x00CD754C;
constexpr uintptr_t kScenePointLightBuckets = 0x24;
constexpr uint32_t kPointLightBucketCount = 4096;
constexpr uint32_t kMaxPointLightBucketNodes = 512;
constexpr uint32_t kMaxPointLightNodes = 8192;
constexpr uint32_t kNativePointLightType = 1;
constexpr float kMaxWorldCoordinate = 1000000.0f;
constexpr float kMaxLightComponent = 10000.0f;
constexpr float kMaxAttenuationCoefficient = 1000000.0f;
constexpr uintptr_t kCameraWmoInstance = 0x00CD87A4;
constexpr uintptr_t kCameraWmoGroupCount = 0x00CDB0D8;
constexpr uintptr_t kCameraWmoGroupIds = 0x00CDB0DC;
constexpr uintptr_t kCameraInteriorFogBlend = 0x00D38B9C;
constexpr uintptr_t kWmoInstanceRoot = 0xF4;
constexpr uintptr_t kWmoRootLoaded = 0x1E0;
constexpr uintptr_t kWmoRootGroupCount = 0x1F4;
constexpr uintptr_t kWmoRootGroups = 0x1F8;
constexpr uintptr_t kWmoGroupLoaded = 0x198;
constexpr uintptr_t kWmoGroupFlags = 0x30;
constexpr uint32_t kWmoExteriorFogFlags = 0x48;
constexpr uint32_t kMaxCameraWmoGroups = 64;
constexpr uint32_t kMaxWmoGroups = 4096;

struct NativePointLight
{
    uintptr_t scene;
    uint32_t frameStamp;
    uint32_t type;
    float position[3];
    float viewPosition[3];
    float direction[3];
    float ambient[3];
    float color[3];
    float specular[3];
    float attenuation[3];
    uint32_t enabled;
    uintptr_t previousLink;
    uintptr_t next;
};

static_assert(sizeof(NativePointLight) == 0x6C);
static_assert(offsetof(NativePointLight, position) == 0xC);
static_assert(offsetof(NativePointLight, color) == 0x3C);
static_assert(offsetof(NativePointLight, attenuation) == 0x54);
static_assert(offsetof(NativePointLight, enabled) == 0x60);
static_assert(offsetof(NativePointLight, next) == 0x68);

template <typename T>
T Read(uintptr_t address)
{
    T value;
    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value));
    return value;
}

bool ValidPointer(uintptr_t address, size_t size)
{
    return address >= 0x10000 && !(address & 3) && address < 0xFFF00000 && size <= 0xFFF00000 - address;
}

bool ValidVector(const float values[3], float minimum, float maximum)
{
    for (int i = 0; i < 3; ++i)
        if (!std::isfinite(values[i]) || values[i] < minimum || values[i] > maximum)
            return false;
    return true;
}

double SquaredDistance(const float a[3], const float b[3])
{
    double result = 0.0;
    for (int i = 0; i < 3; ++i)
    {
        const double delta = static_cast<double>(a[i]) - b[i];
        result += delta * delta;
    }
    return result;
}

float PeakColor(const float color[3])
{
    return std::max(color[0], std::max(color[1], color[2]));
}

double LightPriority(const LocalPointLight& light, const float cameraPosition[3])
{
    const double distanceSquared = SquaredDistance(light.position, cameraPosition);
    const double denominator = light.attenuation[0] + light.attenuation[1] * std::sqrt(distanceSquared) +
                               light.attenuation[2] * distanceSquared;
    return PeakColor(light.color) / std::max(denominator, 1.0);
}

bool Precedes(const LocalPointLight& a, const LocalPointLight& b, const float cameraPosition[3])
{
    const double aPriority = LightPriority(a, cameraPosition);
    const double bPriority = LightPriority(b, cameraPosition);
    if (aPriority != bPriority)
        return aPriority > bPriority;
    for (int i = 0; i < 3; ++i)
        if (a.position[i] != b.position[i])
            return a.position[i] < b.position[i];
    for (int i = 0; i < 3; ++i)
        if (a.color[i] != b.color[i])
            return a.color[i] > b.color[i];
    for (int i = 0; i < 3; ++i)
        if (a.attenuation[i] != b.attenuation[i])
            return a.attenuation[i] < b.attenuation[i];
    return false;
}

template <size_t Size>
bool Matches(uintptr_t address, const unsigned char (&expected)[Size])
{
    return std::memcmp(reinterpret_cast<const void*>(address), expected, Size) == 0;
}

bool SupportedLayoutUnsafe()
{
    const auto* base = reinterpret_cast<const unsigned char*>(GetModuleHandleA(nullptr));
    if (reinterpret_cast<uintptr_t>(base) != kClientImageBase)
        return false;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 || dos->e_lfanew > 0x1000)
        return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.TimeDateStamp != kClientTimestamp ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
        return false;
    const unsigned char sceneLoad[] = {0x8B, 0x3D, 0x4C, 0x75, 0xCD, 0x00};
    const unsigned char pointTypeAndBuckets[] = {
        0x83, 0x7E, 0x08, 0x01, 0x0F, 0x85, 0xC6, 0x00, 0x00, 0x00, 0x83, 0x78, 0x24, 0x00};
    const unsigned char pointLinks[] = {0x8D, 0x0C, 0xB8, 0x89, 0x4E, 0x64, 0x8B, 0x09, 0x8B, 0x56, 0x64};
    const unsigned char attenuationUpload[] = {
        0xD9, 0x40, 0x54, 0xD9, 0x5D, 0xD0, 0xD9, 0x40, 0x58, 0xD9, 0x5D, 0xD4, 0xD9, 0x40, 0x5C};
    const unsigned char interiorBlendStore[] = {0xD9, 0x15, 0x9C, 0x8B, 0xD3, 0x00};
    const unsigned char cameraGroups[] = {0x8B, 0x0D, 0xDC, 0xB0, 0xCD, 0x00, 0x8B, 0x14, 0xB1};
    const unsigned char interiorFlags[] = {0xF6, 0x40, 0x30, 0x48, 0x75, 0x23};
    const unsigned char loadedGroups[] = {
        0x83, 0xB9, 0xE0, 0x01, 0x00, 0x00, 0x00, 0x74, 0x19, 0x8B, 0x45, 0x08,
        0x8B, 0x84, 0x81, 0xF8, 0x01, 0x00, 0x00};
    return Matches(0x004F90EC, sceneLoad) && Matches(0x00834C8D, pointTypeAndBuckets) &&
           Matches(0x00834D3C, pointLinks) && Matches(0x00835539, attenuationUpload) &&
           Matches(0x007F1931, interiorBlendStore) && Matches(0x007A11B0, cameraGroups) &&
           Matches(0x007A11C7, interiorFlags) && Matches(0x007AEA83, loadedGroups);
}

bool SupportedLayout()
{
    __try
    {
        return SupportedLayoutUnsafe();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool CapturePointLightsUnsafe(const float cameraPosition[3], LocalLightInputs& out)
{
    const uintptr_t scene = Read<uintptr_t>(kWorldM2Scene);
    if (!scene)
        return true;
    if (!ValidPointer(scene, kScenePointLightBuckets + sizeof(uintptr_t)))
        return false;
    const uintptr_t buckets = Read<uintptr_t>(scene + kScenePointLightBuckets);
    if (!buckets)
        return true;
    if (!ValidPointer(buckets, kPointLightBucketCount * sizeof(uintptr_t)))
        return false;
    uint32_t nodeCount = 0;
    for (uint32_t bucket = 0; bucket < kPointLightBucketCount; ++bucket)
    {
        uintptr_t previousLink = buckets + bucket * sizeof(uintptr_t);
        uintptr_t address = Read<uintptr_t>(previousLink);
        uint32_t bucketNodes = 0;
        while (address)
        {
            if (++bucketNodes > kMaxPointLightBucketNodes || ++nodeCount > kMaxPointLightNodes ||
                !ValidPointer(address, sizeof(NativePointLight)))
                return false;
            const NativePointLight native = Read<NativePointLight>(address);
            if (native.scene != scene || native.previousLink != previousLink ||
                native.type != kNativePointLightType || !native.enabled)
                return false;
            LocalPointLight light;
            std::memcpy(light.position, native.position, sizeof(light.position));
            std::memcpy(light.color, native.color, sizeof(light.color));
            std::memcpy(light.attenuation, native.attenuation, sizeof(light.attenuation));
            engine::SelectLocalPointLight(out, light, cameraPosition);
            previousLink = address + offsetof(NativePointLight, next);
            address = native.next;
        }
    }
    return Read<uintptr_t>(kWorldM2Scene) == scene && Read<uintptr_t>(scene + kScenePointLightBuckets) == buckets;
}

bool CaptureInteriorUnsafe(LocalLightInputs& out)
{
    const uintptr_t instance = Read<uintptr_t>(kCameraWmoInstance);
    const uint32_t count = Read<uint32_t>(kCameraWmoGroupCount);
    if (!instance || !count)
        return true;
    if (count > kMaxCameraWmoGroups || !ValidPointer(instance, kWmoInstanceRoot + sizeof(uintptr_t)))
        return false;
    const uintptr_t root = Read<uintptr_t>(instance + kWmoInstanceRoot);
    if (!ValidPointer(root, kWmoRootGroups) || !Read<uint32_t>(root + kWmoRootLoaded))
        return false;
    const uint32_t groupCount = Read<uint32_t>(root + kWmoRootGroupCount);
    const uintptr_t ids = Read<uintptr_t>(kCameraWmoGroupIds);
    if (!groupCount || groupCount > kMaxWmoGroups || !ValidPointer(ids, count * sizeof(uint32_t)) ||
        !ValidPointer(root, kWmoRootGroups + groupCount * sizeof(uintptr_t)))
        return false;
    for (uint32_t i = 0; i < count; ++i)
    {
        const uint32_t groupId = Read<uint32_t>(ids + i * sizeof(uint32_t));
        if (groupId >= groupCount)
            return false;
        const uintptr_t group = Read<uintptr_t>(root + kWmoRootGroups + groupId * sizeof(uintptr_t));
        if (!ValidPointer(group, kWmoGroupLoaded + sizeof(uint32_t)))
            return false;
        if ((Read<uint32_t>(group + kWmoGroupLoaded) & 1) &&
            !(Read<uint32_t>(group + kWmoGroupFlags) & kWmoExteriorFogFlags))
            out.cameraInterior = true;
    }
    const float weight = Read<float>(kCameraInteriorFogBlend);
    if (out.cameraInterior && std::isfinite(weight))
        out.interiorBlend = std::clamp(weight, 0.0f, 1.0f);
    return Read<uintptr_t>(kCameraWmoInstance) == instance && Read<uint32_t>(kCameraWmoGroupCount) == count &&
           Read<uintptr_t>(kCameraWmoGroupIds) == ids;
}

bool CaptureInputsGuarded(const float cameraPosition[3], LocalLightInputs& out)
{
    __try
    {
        return CapturePointLightsUnsafe(cameraPosition, out) && CaptureInteriorUnsafe(out);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
}

namespace engine
{
float PointLightCutoff(const float color[3], const float attenuation[3])
{
    if (!ValidVector(color, 0.0f, kMaxLightComponent) ||
        !ValidVector(attenuation, 0.0f, kMaxAttenuationCoefficient))
        return 0.0f;
    const double peak = PeakColor(color);
    const double target = peak / kLocalPointLightContributionCutoff;
    if (peak <= 0.0 || target <= std::max(static_cast<double>(attenuation[0]), 1.0))
        return 0.0f;
    const double remaining = target - attenuation[0];
    double radius = kMaxLocalPointLightRadius;
    if (attenuation[2] > 0.0f)
    {
        const double linear = attenuation[1];
        radius = 2.0 * remaining /
                 (linear + std::sqrt(linear * linear + 4.0 * attenuation[2] * remaining));
    }
    else if (attenuation[1] > 0.0f)
        radius = remaining / attenuation[1];
    return static_cast<float>(std::min(radius, static_cast<double>(kMaxLocalPointLightRadius)));
}

bool SelectLocalPointLight(LocalLightInputs& out, const LocalPointLight& light, const float cameraPosition[3])
{
    if (out.pointLightCount > kMaxLocalPointLights ||
        !ValidVector(cameraPosition, -kMaxWorldCoordinate, kMaxWorldCoordinate) ||
        !ValidVector(light.position, -kMaxWorldCoordinate, kMaxWorldCoordinate))
        return false;
    LocalPointLight candidate = light;
    candidate.cutoff = PointLightCutoff(candidate.color, candidate.attenuation);
    const double limit = candidate.cutoff + kMaxLocalPointLightRadius;
    if (candidate.cutoff <= 0.0f || SquaredDistance(candidate.position, cameraPosition) > limit * limit)
        return false;
    uint32_t index = 0;
    while (index < out.pointLightCount && !Precedes(candidate, out.pointLights[index], cameraPosition))
        ++index;
    if (index == kMaxLocalPointLights)
        return false;
    const uint32_t count = std::min(out.pointLightCount + 1, kMaxLocalPointLights);
    for (uint32_t i = count - 1; i > index; --i)
        out.pointLights[i] = out.pointLights[i - 1];
    out.pointLights[index] = candidate;
    out.pointLightCount = count;
    return true;
}

bool CaptureLocalLightInputs(const float cameraPosition[3], LocalLightInputs& out)
{
    out = {};
    static const bool supported = SupportedLayout();
    if (!supported || !ValidVector(cameraPosition, -kMaxWorldCoordinate, kMaxWorldCoordinate))
        return false;
    LocalLightInputs captured;
    const bool valid = CaptureInputsGuarded(cameraPosition, captured);
    if (valid)
        out = captured;
    return valid;
}
}
