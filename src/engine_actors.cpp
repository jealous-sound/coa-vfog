#include "engine_actors.h"

#include "log.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>

namespace
{
constexpr uintptr_t kClientImageBase = 0x00400000;
constexpr uint32_t kClientTimestamp = 0x4C2452FE;
constexpr uintptr_t kClientConnection = 0x00C79CE0;
constexpr uintptr_t kConnectionObjectManager = 0x2ED0;
constexpr uintptr_t kObjectManagerSize = 0xD8;
constexpr uintptr_t kManagerVisibleLinkOffset = 0xA4;
constexpr uintptr_t kManagerVisibleHead = 0xA8;
constexpr uintptr_t kManagerVisibleFirst = 0xAC;
constexpr uint32_t kVisibleListLinkOffset = 0x38;
constexpr uintptr_t kListEndMarker = 1;
constexpr uint32_t kMaxVisibleObjects = 4096;
constexpr uintptr_t kObjectDescriptors = 0x08;
constexpr uintptr_t kObjectGuid = 0x30;
constexpr uintptr_t kObjectPreviousLink = kVisibleListLinkOffset;
constexpr uintptr_t kObjectNext = kVisibleListLinkOffset + sizeof(uintptr_t);
constexpr uintptr_t kObjectWorldEntity = 0xB8;
constexpr uintptr_t kDescriptorTypeMask = 0x08;
constexpr uintptr_t kDescriptorScale = 0x10;
constexpr uint32_t kUnitTypeBit = 0x8;
constexpr uintptr_t kUnitMovement = 0xD8;
constexpr uintptr_t kUnitMovementBlock = 0x788;
constexpr uintptr_t kUnitTransportGuid = 0x790;
constexpr uintptr_t kUnitPosition = 0x798;
constexpr uintptr_t kUnitMovementFlags = 0x7CC;
constexpr uintptr_t kUnitCurrentSpeed = 0x814;
constexpr uintptr_t kUnitCollisionRadius = 0x850;
constexpr uintptr_t kUnitCollisionHeight = 0x854;
constexpr uintptr_t kUnitNextRippleTime = 0xA58;
constexpr uintptr_t kUnitSize = kUnitNextRippleTime + sizeof(uint32_t);
constexpr uint32_t kSwimmingMovementFlag = 0x200000;
constexpr uintptr_t kEntityWorldPosition = 0x6C;
constexpr uintptr_t kEntityLiquidFlags = 0x7C;
constexpr uintptr_t kEntityLiquidSurface = 0x80;
constexpr uintptr_t kEntitySize = 0xC0;
constexpr uint32_t kEntityLiquidFound = 0x20;
constexpr uint32_t kEntitySurfaceCrossesBody = 0x100;
constexpr float kMaxWorldCoordinate = 100000.0f;
constexpr float kMaxCollisionRadius = 50.0f;
constexpr float kMaxCollisionHeight = 100.0f;
constexpr float kMaxUnitSpeed = 1000.0f;
constexpr float kMaxObjectScale = 1000.0f;
constexpr float kMinClientRippleDepthLimit = 1.0f;
constexpr float kClientRippleDepthLimitPerHeight = 2.0f;
constexpr size_t kMaxGuardBytes = 20;

struct GuardBytes
{
    const char* name;
    uintptr_t address;
    size_t size;
    unsigned char bytes[kMaxGuardBytes];
};

const GuardBytes kWaterContactLayout[] = {
    {"object manager store", 0x004D77A9, 12, {0x8B, 0x0D, 0xE0, 0x9C, 0xC7, 0x00, 0x89, 0x81, 0xD0, 0x2E, 0x00, 0x00}},
    {"visible list first object", 0x004D4B44, 14,
     {0x8B, 0x86, 0x08, 0x00, 0x00, 0x00, 0x05, 0xA8, 0x00, 0x00, 0x00, 0x8B, 0x40, 0x04}},
    {"visible list next object", 0x004D4B80, 18,
     {0x8B, 0x86, 0x08, 0x00, 0x00, 0x00, 0x05, 0xA4, 0x00, 0x00, 0x00, 0x8B, 0x00, 0x03, 0xC3, 0x8B, 0x58, 0x04}},
    {"visible list link offset", 0x004D6193, 5, {0xBA, 0x38, 0x00, 0x00, 0x00}},
    {"visible list link offset store", 0x004D6233, 6, {0x89, 0x96, 0xA4, 0x00, 0x00, 0x00}},
    {"object guid", 0x004D4B6D, 6, {0x8B, 0x4B, 0x34, 0x8B, 0x43, 0x30}},
    {"object type mask", 0x004D4DF1, 9, {0x8B, 0x48, 0x08, 0x8B, 0x55, 0x10, 0x85, 0x51, 0x08}},
    {"unit movement block", 0x0073F67A, 12, {0x8D, 0x86, 0x88, 0x07, 0x00, 0x00, 0x89, 0x86, 0xD8, 0x00, 0x00, 0x00}},
    {"unit position", 0x006E6F13, 9, {0x8B, 0x89, 0xD8, 0x00, 0x00, 0x00, 0x8B, 0x51, 0x10}},
    {"unit transport guid", 0x006E6F70, 12, {0x8B, 0x81, 0x90, 0x07, 0x00, 0x00, 0x8B, 0x91, 0x94, 0x07, 0x00, 0x00}},
    {"unit swimming flag", 0x00730DA2, 8, {0x8B, 0x40, 0x44, 0xA9, 0x00, 0x00, 0x20, 0x00}},
    {"unit current speed", 0x0071CCA8, 17,
     {0x8B, 0x86, 0xD8, 0x00, 0x00, 0x00, 0xD9, 0xC0, 0xD9, 0x55, 0xE0, 0xD9, 0x80, 0x8C, 0x00, 0x00, 0x00}},
    {"unit collision radius store", 0x006E95AF, 6, {0xD9, 0x9E, 0xC8, 0x00, 0x00, 0x00}},
    {"unit collision height store", 0x006E95CE, 6, {0xD9, 0x9E, 0xCC, 0x00, 0x00, 0x00}},
    {"unit ripple depth limit", 0x0071CC28, 10, {0xD9, 0x86, 0x54, 0x08, 0x00, 0x00, 0xDC, 0xC0, 0xD9, 0xE8}},
    {"object world entity store", 0x007438B0, 6, {0x89, 0x86, 0xB8, 0x00, 0x00, 0x00}},
    {"entity liquid surface", 0x0077F1EA, 12, {0xF6, 0x40, 0x7C, 0x20, 0x74, 0x27, 0xD9, 0x80, 0x80, 0x00, 0x00, 0x00}},
    {"entity liquid body crossing", 0x0077F230, 12,
     {0x8B, 0x48, 0x7C, 0x8B, 0x55, 0x0C, 0xC1, 0xE9, 0x08, 0x83, 0xE1, 0x01}},
    {"entity world position store", 0x007803BD, 12,
     {0x89, 0x79, 0x6C, 0xD8, 0x65, 0xBC, 0x89, 0x59, 0x70, 0x89, 0x51, 0x74}},
    {"object scale", 0x004D5F00, 7, {0x8B, 0x41, 0x08, 0xD9, 0x40, 0x10, 0xC3}},
    {"unit ripple size from its scale", 0x0071CD08, 15,
     {0x8B, 0x16, 0x8B, 0x42, 0x3C, 0x8B, 0xCE, 0xFF, 0xD0, 0xD8, 0x0D, 0x98, 0x20, 0xA1, 0x00}},
    {"unit ripple kind from the movement flags", 0x0071CC0B, 11,
     {0x8B, 0x96, 0xD8, 0x00, 0x00, 0x00, 0x8B, 0x42, 0x44, 0xA8, 0x0F}},
    {"unit next ripple time check", 0x0071CC71, 14,
     {0x8B, 0x86, 0x58, 0x0A, 0x00, 0x00, 0x85, 0xC0, 0x8B, 0x0D, 0xAC, 0x76, 0xCD, 0x00}},
    {"unit ripple emission", 0x0071CF16, 5, {0xE8, 0xE5, 0x24, 0x06, 0x00}},
    {"unit moving ripple time store", 0x0071CF58, 6, {0x89, 0x86, 0x58, 0x0A, 0x00, 0x00}},
    {"unit ripple time store", 0x0071CF8D, 6, {0x89, 0x96, 0x58, 0x0A, 0x00, 0x00}},
};

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

bool FiniteWithin(float value, float minimum, float maximum)
{
    return std::isfinite(value) && value >= minimum && value <= maximum;
}

bool PlausibleContact(const WaterContact& c)
{
    for (float axis : c.position)
        if (!FiniteWithin(axis, -kMaxWorldCoordinate, kMaxWorldCoordinate))
            return false;
    return FiniteWithin(c.surface, -kMaxWorldCoordinate, kMaxWorldCoordinate) && c.radius > 0.0f &&
           FiniteWithin(c.radius, 0.0f, kMaxCollisionRadius) && c.height > 0.0f &&
           FiniteWithin(c.height, 0.0f, kMaxCollisionHeight) && FiniteWithin(c.speed, 0.0f, kMaxUnitSpeed) &&
           FiniteWithin(c.scale, 0.0f, kMaxObjectScale) && WaterContactDepth(c) < ClientRippleDepthLimit(c.height);
}

double SquaredGroundDistance(const WaterContact& c, const float centre[3])
{
    const double dx = static_cast<double>(c.position[0]) - centre[0];
    const double dy = static_cast<double>(c.position[1]) - centre[1];
    return dx * dx + dy * dy;
}

bool Precedes(const WaterContact& a, const WaterContact& b, const float centre[3])
{
    const double aDistance = SquaredGroundDistance(a, centre);
    const double bDistance = SquaredGroundDistance(b, centre);
    if (aDistance != bDistance)
        return aDistance < bDistance;
    return a.guid < b.guid;
}

bool LayoutMatchesUnsafe()
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
    for (const GuardBytes& guard : kWaterContactLayout)
        if (std::memcmp(reinterpret_cast<const void*>(guard.address), guard.bytes, guard.size) != 0)
        {
            VF_LOG_ERROR("water contacts: the client's %s at 0x%08X differs from the 12340 client; ripples get no "
                         "contacts",
                         guard.name, static_cast<unsigned>(guard.address));
            return false;
        }
    return true;
}

bool LayoutMatches()
{
    __try
    {
        return LayoutMatchesUnsafe();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool IsUnit(uintptr_t object)
{
    const uintptr_t descriptors = Read<uintptr_t>(object + kObjectDescriptors);
    return ValidPointer(descriptors, kDescriptorScale + sizeof(float)) &&
           (Read<uint32_t>(descriptors + kDescriptorTypeMask) & kUnitTypeBit) && ValidPointer(object, kUnitSize) &&
           Read<uintptr_t>(object + kUnitMovement) == object + kUnitMovementBlock;
}

bool ReadWaterContactUnsafe(uintptr_t object, WaterContact& contact)
{
    if (!IsUnit(object))
        return false;
    const uintptr_t entity = Read<uintptr_t>(object + kObjectWorldEntity);
    if (!ValidPointer(entity, kEntitySize))
        return false;
    const uint32_t liquid = Read<uint32_t>(entity + kEntityLiquidFlags);
    if (!(liquid & kEntityLiquidFound) || !(liquid & kEntitySurfaceCrossesBody))
        return false;
    contact.guid = Read<uint64_t>(object + kObjectGuid);
    contact.onTransport = Read<uint64_t>(object + kUnitTransportGuid) != 0;
    const uintptr_t position = contact.onTransport ? entity + kEntityWorldPosition : object + kUnitPosition;
    std::memcpy(contact.position, reinterpret_cast<const void*>(position), sizeof(contact.position));
    contact.surface = Read<float>(entity + kEntityLiquidSurface);
    contact.radius = Read<float>(object + kUnitCollisionRadius);
    contact.height = Read<float>(object + kUnitCollisionHeight);
    contact.speed = Read<float>(object + kUnitCurrentSpeed);
    contact.scale = Read<float>(Read<uintptr_t>(object + kObjectDescriptors) + kDescriptorScale);
    contact.movementFlags = Read<uint32_t>(object + kUnitMovementFlags);
    contact.nextRippleMs = Read<uint32_t>(object + kUnitNextRippleTime);
    contact.swimming = (contact.movementFlags & kSwimmingMovementFlag) != 0;
    return true;
}

bool WalkVisibleUnitsUnsafe(uintptr_t manager, const float centre[3], WaterContactFrame& out)
{
    if (!ValidPointer(manager, kObjectManagerSize) ||
        Read<uint32_t>(manager + kManagerVisibleLinkOffset) != kVisibleListLinkOffset)
        return false;
    uintptr_t previousLink = manager + kManagerVisibleHead;
    uintptr_t object = Read<uintptr_t>(manager + kManagerVisibleFirst);
    for (uint32_t visited = 0; object && !(object & kListEndMarker); ++visited)
    {
        if (visited == kMaxVisibleObjects)
        {
            out.truncated = true;
            return true;
        }
        if (!ValidPointer(object, kObjectNext + sizeof(uintptr_t)) ||
            Read<uintptr_t>(object + kObjectPreviousLink) != previousLink)
            return false;
        WaterContact contact;
        if (ReadWaterContactUnsafe(object, contact))
            engine::SelectWaterContact(out, contact, centre);
        previousLink = object + kObjectPreviousLink;
        object = Read<uintptr_t>(object + kObjectNext);
    }
    return true;
}

bool WalkVisibleUnits(uintptr_t manager, const float centre[3], WaterContactFrame& out)
{
    __try
    {
        return WalkVisibleUnitsUnsafe(manager, centre, out);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool ReadObjectManager(uintptr_t& manager)
{
    __try
    {
        manager = 0;
        const uintptr_t connection = Read<uintptr_t>(kClientConnection);
        if (!connection)
            return true;
        if (!ValidPointer(connection, kConnectionObjectManager + sizeof(uintptr_t)))
            return false;
        manager = Read<uintptr_t>(connection + kConnectionObjectManager);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool ValidCentre(const float centre[3])
{
    for (int i = 0; i < 3; ++i)
        if (!FiniteWithin(centre[i], -kMaxWorldCoordinate, kMaxWorldCoordinate))
            return false;
    return true;
}
}

float WaterContactDepth(const WaterContact& contact)
{
    return contact.surface - contact.position[2];
}

float ClientRippleDepthLimit(float height)
{
    return std::max(kMinClientRippleDepthLimit, kClientRippleDepthLimitPerHeight * height);
}

namespace engine
{
bool SelectWaterContact(WaterContactFrame& frame, const WaterContact& contact, const float centre[3])
{
    if (frame.count > kMaxWaterContacts || !ValidCentre(centre) || !PlausibleContact(contact) ||
        SquaredGroundDistance(contact, centre) > static_cast<double>(kWaterContactRange) * kWaterContactRange)
        return false;
    uint32_t index = 0;
    while (index < frame.count && !Precedes(contact, frame.contacts[index], centre))
        ++index;
    if (index == kMaxWaterContacts)
        return false;
    const uint32_t count = std::min(frame.count + 1, kMaxWaterContacts);
    for (uint32_t i = count - 1; i > index; --i)
        frame.contacts[i] = frame.contacts[i - 1];
    frame.contacts[index] = contact;
    frame.count = count;
    return true;
}

bool CaptureWaterContactsFrom(uintptr_t objectManager, const float centre[3], WaterContactFrame& out)
{
    out = {};
    if (!ValidCentre(centre))
        return false;
    if (!objectManager)
        return true;
    WaterContactFrame captured;
    const bool valid = WalkVisibleUnits(objectManager, centre, captured);
    if (valid)
        out = captured;
    return valid;
}

bool CaptureWaterContacts(const float centre[3], WaterContactFrame& out)
{
    out = {};
    static const bool supported = LayoutMatches();
    uintptr_t manager = 0;
    if (!supported || !ValidCentre(centre) || !ReadObjectManager(manager))
        return false;
    if (!CaptureWaterContactsFrom(manager, centre, out))
        return false;
    uintptr_t after = 0;
    if (ReadObjectManager(after) && after == manager)
        return true;
    out = {};
    return false;
}
}
