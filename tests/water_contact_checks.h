#pragma once

namespace water_contact_checks
{
constexpr double kStartSeconds = 100.0;
constexpr float kSurfaceZ = 10.0f;
constexpr float kUnitHeight = 2.0f;
constexpr float kUnitRadius = 0.35f;
constexpr float kWadingDepth = 0.5f;
constexpr float kRunSpeed = 7.0f;
constexpr uintptr_t kManagerSize = 0xD8;
constexpr uintptr_t kManagerLinkOffset = 0xA4;
constexpr uintptr_t kManagerHeadLink = 0xA8;
constexpr uint32_t kListLinkOffset = 0x38;
constexpr uintptr_t kListEnd = 1;
constexpr uintptr_t kObjectDescriptors = 0x08;
constexpr uintptr_t kObjectGuid = 0x30;
constexpr uintptr_t kObjectWorldEntity = 0xB8;
constexpr uintptr_t kUnitMovement = 0xD8;
constexpr uintptr_t kUnitMovementBlock = 0x788;
constexpr uintptr_t kUnitTransportGuid = 0x790;
constexpr uintptr_t kUnitPosition = 0x798;
constexpr uintptr_t kUnitMovementFlags = 0x7CC;
constexpr uintptr_t kUnitSpeed = 0x814;
constexpr uintptr_t kUnitRadiusField = 0x850;
constexpr uintptr_t kUnitHeightField = 0x854;
constexpr uintptr_t kUnitNextRippleField = 0xA58;
constexpr size_t kUnitBytes = 0xA60;
constexpr size_t kSmallObjectBytes = 0x40;
constexpr size_t kDescriptorBytes = 0x20;
constexpr uintptr_t kDescriptorTypeMask = 0x08;
constexpr uintptr_t kDescriptorScale = 0x10;
constexpr uintptr_t kEntityPosition = 0x6C;
constexpr uintptr_t kEntityLiquidFlags = 0x7C;
constexpr uintptr_t kEntitySurface = 0x80;
constexpr size_t kEntityBytes = 0xC0;
constexpr uint32_t kUnitTypeMask = 0x9;
constexpr uint32_t kPlayerTypeMask = 0x19;
constexpr uint32_t kGameObjectTypeMask = 0x21;
constexpr uint32_t kItemTypeMask = 0x3;
constexpr uint32_t kInWaterLiquidFlags = 0x20 | 0x100;
constexpr uint32_t kLiquidBelowFlags = 0x20;
constexpr uint32_t kSwimmingFlag = 0x200000;
constexpr uint32_t kForwardFlag = 0x1;
constexpr float kLargeScale = 1.5f;
constexpr uint32_t kWorldMs = 7200000;
constexpr uint32_t kCappedObjects = 4100;
constexpr uint32_t kDisturbanceCapacity = 32;
constexpr double kStepSeconds = 1.0 / 30.0;
constexpr uint64_t kCrowdOverflow = 44;

template <typename T>
void Put(uintptr_t address, T value)
{
    std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(value));
}

struct UnitFixture
{
    uint64_t guid = 0;
    float position[3] = {};
    float entityPosition[3] = {};
    float surface = kSurfaceZ;
    float radius = kUnitRadius;
    float height = kUnitHeight;
    float speed = 0.0f;
    float scale = 1.0f;
    uint32_t movementFlags = 0;
    uint32_t nextRippleMs = 0;
    uint32_t liquidFlags = kInWaterLiquidFlags;
    uint32_t typeMask = kUnitTypeMask;
    uint64_t transportGuid = 0;
    bool entity = true;
    bool movementInBlock = true;
};

UnitFixture WadingUnit(uint64_t guid, float x, float y)
{
    UnitFixture unit;
    unit.guid = guid;
    unit.position[0] = x;
    unit.position[1] = y;
    unit.position[2] = kSurfaceZ - kWadingDepth;
    std::copy(unit.position, unit.position + 3, unit.entityPosition);
    unit.speed = kRunSpeed;
    return unit;
}

class SyntheticObjects
{
public:
    SyntheticObjects()
    {
        m_manager = Allocate(kManagerSize);
        Put<uint32_t>(m_manager + kManagerLinkOffset, kListLinkOffset);
        m_lastLink = m_manager + kManagerHeadLink;
        Put<uintptr_t>(m_lastLink + sizeof(uintptr_t), End());
    }

    uintptr_t Manager() const { return m_manager; }

    uintptr_t AddObject(size_t bytes, uint32_t typeMask, float scale = 1.0f)
    {
        const uintptr_t object = Allocate(bytes);
        const uintptr_t descriptors = Allocate(kDescriptorBytes);
        Put<uint32_t>(descriptors + kDescriptorTypeMask, typeMask);
        Put<float>(descriptors + kDescriptorScale, scale);
        Put<uintptr_t>(object + kObjectDescriptors, descriptors);
        Put<uintptr_t>(object + kListLinkOffset, m_lastLink);
        Put<uintptr_t>(object + kListLinkOffset + sizeof(uintptr_t), End());
        Put<uintptr_t>(m_lastLink + sizeof(uintptr_t), object);
        m_lastLink = object + kListLinkOffset;
        return object;
    }

    uintptr_t AddUnit(const UnitFixture& unit)
    {
        const uintptr_t object = AddObject(kUnitBytes, unit.typeMask, unit.scale);
        Put<uint64_t>(object + kObjectGuid, unit.guid);
        Put<uintptr_t>(object + kUnitMovement, unit.movementInBlock ? object + kUnitMovementBlock : object);
        Put<uint64_t>(object + kUnitTransportGuid, unit.transportGuid);
        std::memcpy(reinterpret_cast<void*>(object + kUnitPosition), unit.position, sizeof(unit.position));
        Put<uint32_t>(object + kUnitMovementFlags, unit.movementFlags);
        Put<float>(object + kUnitSpeed, unit.speed);
        Put<float>(object + kUnitRadiusField, unit.radius);
        Put<float>(object + kUnitHeightField, unit.height);
        Put<uint32_t>(object + kUnitNextRippleField, unit.nextRippleMs);
        if (!unit.entity)
            return object;
        const uintptr_t entity = Allocate(kEntityBytes);
        std::memcpy(reinterpret_cast<void*>(entity + kEntityPosition), unit.entityPosition,
                    sizeof(unit.entityPosition));
        Put<uint32_t>(entity + kEntityLiquidFlags, unit.liquidFlags);
        Put<float>(entity + kEntitySurface, unit.surface);
        Put<uintptr_t>(object + kObjectWorldEntity, entity);
        return object;
    }

private:
    uintptr_t Allocate(size_t bytes)
    {
        m_blocks.emplace_back((bytes + sizeof(uint32_t) - 1) / sizeof(uint32_t), 0u);
        return reinterpret_cast<uintptr_t>(m_blocks.back().data());
    }

    uintptr_t End() const { return (m_manager + kManagerHeadLink) | kListEnd; }

    std::vector<std::vector<uint32_t>> m_blocks;
    uintptr_t m_manager = 0;
    uintptr_t m_lastLink = 0;
};

const WaterContact* ContactOf(const WaterContactFrame& frame, uint64_t guid)
{
    for (uint32_t i = 0; i < frame.count; ++i)
        if (frame.contacts[i].guid == guid)
            return &frame.contacts[i];
    return nullptr;
}

void CheckVisibleUnitWalk()
{
    const float centre[3] = {0.0f, 0.0f, kSurfaceZ};
    SyntheticObjects world;
    UnitFixture wading = WadingUnit(1, 2.0f, 1.0f);
    wading.movementFlags = kSwimmingFlag | kForwardFlag;
    wading.scale = kLargeScale;
    wading.nextRippleMs = kWorldMs;
    world.AddUnit(wading);
    UnitFixture player = WadingUnit(2, -3.0f, 0.5f);
    player.typeMask = kPlayerTypeMask;
    world.AddUnit(player);
    UnitFixture passenger = WadingUnit(3, 0.25f, 0.5f);
    passenger.transportGuid = 0xF120000000001234ull;
    const float deck[3] = {6.0f, -4.0f, kSurfaceZ - 0.3f};
    std::copy(deck, deck + 3, passenger.entityPosition);
    world.AddUnit(passenger);
    UnitFixture dry = WadingUnit(4, 1.0f, 1.0f);
    dry.liquidFlags = 0;
    UnitFixture above = WadingUnit(5, 1.0f, 1.0f);
    above.liquidFlags = kLiquidBelowFlags;
    UnitFixture gameObject = WadingUnit(6, 1.0f, 1.0f);
    gameObject.typeMask = kGameObjectTypeMask;
    UnitFixture detached = WadingUnit(7, 1.0f, 1.0f);
    detached.movementInBlock = false;
    UnitFixture tooDeep = WadingUnit(8, 1.0f, 1.0f);
    tooDeep.position[2] = kSurfaceZ - ClientRippleDepthLimit(kUnitHeight) - 0.1f;
    UnitFixture unloaded = WadingUnit(9, 1.0f, 1.0f);
    unloaded.entity = false;
    UnitFixture broken = WadingUnit(10, 1.0f, 1.0f);
    broken.radius = std::numeric_limits<float>::quiet_NaN();
    UnitFixture unscaled = WadingUnit(11, 1.0f, 1.0f);
    unscaled.scale = std::numeric_limits<float>::quiet_NaN();
    for (const UnitFixture* rejected :
         {&dry, &above, &gameObject, &detached, &tooDeep, &unloaded, &broken, &unscaled})
        world.AddUnit(*rejected);
    WaterContactFrame frame;
    const bool captured = engine::CaptureWaterContactsFrom(world.Manager(), centre, frame);
    const WaterContact* swimmer = ContactOf(frame, 1);
    const WaterContact* rider = ContactOf(frame, 3);
    std::printf("     synthetic object manager: captured %d, %u contacts\n", captured, frame.count);
    Check(captured && frame.count == 3 && swimmer && ContactOf(frame, 2) && rider && !frame.truncated,
          "the unit walk keeps units and players whose entity has liquid (0x20) crossing the body (0x100) and skips "
          "dry units, units above the surface, game objects, units without the embedded movement block or a loaded "
          "model, units deeper than max(1, 2h) and non-finite sizes or scales");
    Check(swimmer && swimmer->position[0] == wading.position[0] && swimmer->surface == kSurfaceZ &&
              swimmer->radius == kUnitRadius && swimmer->height == kUnitHeight && swimmer->speed == kRunSpeed &&
              swimmer->swimming && !swimmer->onTransport && swimmer->scale == kLargeScale &&
              swimmer->movementFlags == wading.movementFlags && swimmer->nextRippleMs == kWorldMs &&
              std::fabs(WaterContactDepth(*swimmer) - kWadingDepth) < 1e-5f,
          "a captured contact carries the raw position, liquid surface, collision radius and height, current speed, "
          "movement flags with the swimming flag, the object scale and the client's next ripple time (+0xA58)");
    Check(rider && rider->onTransport && rider->position[0] == deck[0] && rider->position[1] == deck[1] &&
              rider->position[2] == deck[2],
          "a transport passenger takes its world entity's position instead of its transport-local one");

    SyntheticObjects cut;
    cut.AddUnit(WadingUnit(1, 0.0f, 0.0f));
    const uintptr_t second = cut.AddUnit(WadingUnit(2, 1.0f, 0.0f));
    Put<uintptr_t>(second + kListLinkOffset, second);
    WaterContactFrame cutFrame;
    cutFrame.count = 7;
    const bool cutCaptured = engine::CaptureWaterContactsFrom(cut.Manager(), centre, cutFrame);
    Check(!cutCaptured && cutFrame.count == 0,
          "a visible-list link whose back pointer is not the previous link rejects the whole walk with no contacts");

    SyntheticObjects crowded;
    crowded.AddUnit(WadingUnit(1, 0.0f, 0.0f));
    for (uint32_t i = 0; i < kCappedObjects; ++i)
        crowded.AddObject(kSmallObjectBytes, kItemTypeMask);
    crowded.AddUnit(WadingUnit(2, 1.0f, 0.0f));
    WaterContactFrame crowdedFrame;
    const bool crowdedCaptured = engine::CaptureWaterContactsFrom(crowded.Manager(), centre, crowdedFrame);
    std::printf("     %u objects after the first unit: captured %d, %u contacts, truncated %d\n", kCappedObjects,
                crowdedCaptured, crowdedFrame.count, crowdedFrame.truncated);
    Check(crowdedCaptured && crowdedFrame.truncated && crowdedFrame.count == 1 && ContactOf(crowdedFrame, 1),
          "a visible list longer than the 4096-object cap keeps the contacts found before the cap");

    SyntheticObjects changedLayout;
    changedLayout.AddUnit(WadingUnit(1, 0.0f, 0.0f));
    Put<uint32_t>(changedLayout.Manager() + kManagerLinkOffset, kListLinkOffset + sizeof(uintptr_t));
    WaterContactFrame changedFrame;
    WaterContactFrame noManagerFrame;
    WaterContactFrame liveFrame;
    const bool changedCaptured = engine::CaptureWaterContactsFrom(changedLayout.Manager(), centre, changedFrame);
    const bool noManager = engine::CaptureWaterContactsFrom(0, centre, noManagerFrame);
    const bool live = engine::CaptureWaterContacts(centre, liveFrame);
    Check(!changedCaptured && changedFrame.count == 0 && noManager && noManagerFrame.count == 0 && !live &&
              liveFrame.count == 0,
          "a manager with another list link offset is rejected, no manager (before login) is an empty frame, and the "
          "live capture refuses the harness image");
}

WaterContact ContactAt(uint64_t guid, float x, float y, float depth)
{
    WaterContact contact;
    contact.guid = guid;
    contact.position[0] = x;
    contact.position[1] = y;
    contact.position[2] = kSurfaceZ - depth;
    contact.surface = kSurfaceZ;
    contact.radius = kUnitRadius;
    contact.height = kUnitHeight;
    contact.speed = kRunSpeed;
    return contact;
}

void CheckContactSelection()
{
    const float centre[3] = {0.0f, 0.0f, kSurfaceZ};
    WaterContactFrame forwards;
    WaterContactFrame backwards;
    constexpr int kCandidates = 40;
    for (int i = 0; i < kCandidates; ++i)
    {
        engine::SelectWaterContact(forwards, ContactAt(100 + i, 1.0f + i, 0.0f, kWadingDepth), centre);
        const int j = kCandidates - 1 - i;
        engine::SelectWaterContact(backwards, ContactAt(100 + j, 1.0f + j, 0.0f, kWadingDepth), centre);
    }
    bool nearest = forwards.count == kMaxWaterContacts && backwards.count == kMaxWaterContacts;
    for (uint32_t i = 0; nearest && i < kMaxWaterContacts; ++i)
        nearest = forwards.contacts[i].guid == 100 + i && backwards.contacts[i].guid == 100 + i;
    Check(nearest, "contact selection keeps the 32 units nearest the window centre whatever the list order");
    WaterContactFrame ties;
    engine::SelectWaterContact(ties, ContactAt(9, 3.0f, 0.0f, kWadingDepth), centre);
    engine::SelectWaterContact(ties, ContactAt(5, 0.0f, 3.0f, kWadingDepth), centre);
    WaterContact distant = ContactAt(11, kWaterContactRange + 1.0f, 0.0f, kWadingDepth);
    WaterContact invalid = ContactAt(12, 1.0f, 1.0f, kWadingDepth);
    invalid.position[1] = std::numeric_limits<float>::infinity();
    const bool farSelected = engine::SelectWaterContact(ties, distant, centre);
    const bool invalidSelected = engine::SelectWaterContact(ties, invalid, centre);
    Check(ties.count == 2 && ties.contacts[0].guid == 5 && !farSelected && !invalidSelected,
          "equally near contacts are ordered by GUID; contacts beyond the range or with non-finite positions are "
          "rejected");
}

WaterContactFrame FrameOf(std::initializer_list<WaterContact> contacts)
{
    WaterContactFrame frame;
    for (const WaterContact& contact : contacts)
        frame.contacts[frame.count++] = contact;
    return frame;
}

uint32_t DisturbancesNow(WaterContactTracker& tracker, double seconds, WaterRippleDisturbance* out)
{
    return tracker.DisturbancesAt(seconds, kStepSeconds, out, kDisturbanceCapacity);
}

void CheckStationaryUnitStopsEmitting()
{
    WaterContactTracker tracker;
    WaterRippleDisturbance d[kDisturbanceCapacity];
    const double frame = 1.0 / 60.0;
    double t = kStartSeconds;
    tracker.Update(FrameOf({}), t);
    float x = 0.0f;
    uint32_t moving = 0;
    for (int i = 0; i < 30; ++i)
    {
        t += frame;
        x += static_cast<float>(kRunSpeed * frame);
        tracker.Update(FrameOf({ContactAt(1, x, 0.0f, kWadingDepth)}), t);
        moving += DisturbancesNow(tracker, t, d) ? 1u : 0u;
    }
    t += frame;
    uint32_t still = 0;
    bool emitting = false;
    for (int i = 0; i < 60; ++i, t += frame)
    {
        tracker.Update(FrameOf({ContactAt(1, x, 0.0f, kWadingDepth)}), t);
        still += DisturbancesNow(tracker, t, d);
        emitting = emitting || tracker.Emitting();
    }
    std::printf("     running unit: %u of 30 frames emit; standing unit: %u disturbances in 60 frames\n", moving,
                still);
    Check(moving >= 29 && still == 0 && !emitting,
          "a wading unit's footprint follows its motion and a unit that stops emits nothing more");
}

struct ImpulseLog
{
    int impulses = 0;
    float amplitude = 0.0f;
};

void Descend(WaterContactTracker& tracker, double& t, float depth, bool swimming, ImpulseLog& log)
{
    WaterContact contact = ContactAt(1, 0.0f, 0.0f, depth);
    contact.swimming = swimming;
    tracker.Update(FrameOf({contact}), t);
    WaterRippleDisturbance d[kDisturbanceCapacity];
    const uint32_t count = DisturbancesNow(tracker, t, d);
    for (uint32_t i = 0; i < count; ++i)
    {
        ++log.impulses;
        log.amplitude = d[i].amplitude;
    }
    t += 1.0 / 60.0;
}

void CheckEntryImpulses()
{
    WaterContactTracker tracker;
    double t = kStartSeconds;
    tracker.Update(FrameOf({}), t);
    t += 1.0 / 60.0;
    ImpulseLog entering;
    for (float depth : {0.2f, 0.5f, 0.7f, 0.9f, 1.1f, 1.2f, 1.2f, 1.2f})
        Descend(tracker, t, depth, false, entering);
    ImpulseLog leaving;
    for (float depth : {1.0f, 0.6f, 0.5f})
        Descend(tracker, t, depth, false, leaving);
    ImpulseLog swimming;
    for (float depth : {0.6f, 1.2f, 1.5f, 0.3f})
        Descend(tracker, t, depth, true, swimming);
    std::printf("     entry impulses: %d entering (amplitude %.3f), %d leaving, %d while swimming\n",
                entering.impulses, entering.amplitude, leaving.impulses, swimming.impulses);
    Check(entering.impulses == 1 && entering.amplitude < 0.0f && leaving.impulses == 1 && swimming.impulses == 0,
          "a unit that is not swimming makes one entry impulse when its depth crosses 0.4 of its height, one more "
          "when it crosses back, and none while it swims (the client's 0x730D10 splash rule)");

    WaterContactTracker fresh;
    t = kStartSeconds;
    fresh.Update(FrameOf({ContactAt(1, 0.0f, 0.0f, 1.5f)}), t);
    WaterRippleDisturbance d[kDisturbanceCapacity];
    const uint32_t seeded = DisturbancesNow(fresh, t, d);
    t += 1.0 / 60.0;
    fresh.Update(FrameOf({ContactAt(1, 0.0f, 0.0f, 1.5f), ContactAt(2, 3.0f, 0.0f, 1.5f)}), t);
    const uint32_t appeared = DisturbancesNow(fresh, t, d);
    Check(seeded == 0 && appeared == 1 && d[0].to[0] == 3.0f,
          "units already deep in water when the tracker starts make no impulse; a unit never seen before that appears "
          "deeper than 0.4 of its height afterwards makes one, approximating the client's zero initial depth");
}

struct WadingStage
{
    float depth;
    bool swimming;
    bool present;
    int frames;
};

int ImpulsesOver(WaterContactTracker& tracker, double& t, uint64_t guid, const WadingStage& stage)
{
    int impulses = 0;
    for (int i = 0; i < stage.frames; ++i)
    {
        t += 1.0 / 60.0;
        WaterContact contact = ContactAt(guid, 0.0f, 0.0f, stage.depth);
        contact.swimming = stage.swimming;
        tracker.Update(stage.present ? FrameOf({contact}) : FrameOf({}), t);
        WaterRippleDisturbance d[kDisturbanceCapacity];
        const uint32_t count = DisturbancesNow(tracker, t, d);
        for (uint32_t k = 0; k < count; ++k)
            impulses += d[k].amplitude < 0.0f ? 1 : 0;
    }
    return impulses;
}

void CheckReturningUnitsKeepTheirSplashDepth()
{
    constexpr float kShore = 0.1f * kUnitHeight;
    constexpr float kWaist = 0.7f * kUnitHeight;
    constexpr float kSwim = 0.8f * kUnitHeight;
    constexpr float kStandUp = 0.75f * kUnitHeight;
    constexpr float kDeep = 1.5f * kUnitHeight;
    constexpr int kDiveFrames = 40;
    constexpr int kForgottenFrames = 31 * 60;
    WaterContactTracker diver;
    double t = kStartSeconds;
    ImpulsesOver(diver, t, 1, {kShore, false, true, 2});
    const int wadeIn = ImpulsesOver(diver, t, 1, {kWaist, false, true, 10});
    const int swim = ImpulsesOver(diver, t, 1, {kSwim, true, true, 10});
    const int dive = ImpulsesOver(diver, t, 1, {kDeep, true, false, kDiveFrames});
    const bool dropped = diver.Find(1) == nullptr && diver.RememberedDepths() == 1;
    const int resurface = ImpulsesOver(diver, t, 1, {kSwim, true, true, 10});
    const int standUp = ImpulsesOver(diver, t, 1, {kStandUp, false, true, 10});
    const int wadeOut = ImpulsesOver(diver, t, 1, {kShore, false, true, 10});
    std::printf("     dive and wade out: %d wading in, %d swimming, %d diving (track dropped %d), %d resurfacing, %d "
                "standing up at 0.75h, %d wading out\n",
                wadeIn, swim, dive, dropped, resurface, standUp, wadeOut);
    Check(wadeIn == 1 && swim == 0 && dive == 0 && dropped && resurface == 0 && standUp == 0 && wadeOut == 1,
          "a unit whose track was dropped while it dived keeps its last depth outside the water, so standing up at "
          "0.75 of its height makes no second entry impulse and wading out makes one, as the client's +0x784 does");

    WaterContactTracker arrivals;
    t = kStartSeconds;
    ImpulsesOver(arrivals, t, 1, {kShore, false, true, 2});
    ImpulsesOver(arrivals, t, 1, {kWaist, false, true, 10});
    const int appearedSwimming = ImpulsesOver(arrivals, t, 2, {kSwim, true, true, 10});
    const int arrivingSwimmer = appearedSwimming + ImpulsesOver(arrivals, t, 2, {kStandUp, false, true, 10});
    const int away = ImpulsesOver(arrivals, t, 1, {kWaist, false, false, kForgottenFrames});
    const int forgotten = away + ImpulsesOver(arrivals, t, 1, {kStandUp, false, true, 10});
    std::printf("     unit first seen swimming then standing at 0.75h: %d impulses; waist-deep unit back at 0.75h "
                "after 31 s: %d, %u depths remembered\n",
                arrivingSwimmer, forgotten, arrivals.RememberedDepths());
    Check(arrivingSwimmer == 0 && forgotten == 1 && arrivals.RememberedDepths() == 0,
          "a unit first seen while swimming starts from its current depth, and a remembered depth is forgotten after "
          "30 s, so a unit back in the water later counts as new");

    WaterContactTracker crowd;
    t = kStartSeconds;
    const uint64_t crowdSize = kMaxDepartedWaterContactDepths + kCrowdOverflow;
    for (uint64_t guid = 1; guid <= crowdSize; ++guid)
        ImpulsesOver(crowd, t, guid, {kWaist, false, true, 1});
    ImpulsesOver(crowd, t, 0, {kWaist, false, false, kDiveFrames});
    const uint32_t remembered = crowd.RememberedDepths();
    const int newest = ImpulsesOver(crowd, t, crowdSize, {kStandUp, false, true, 1});
    const int oldest = ImpulsesOver(crowd, t, 1, {kStandUp, false, true, 1});
    std::printf("     %llu waist-deep units passing by: %u depths remembered; back at 0.75h: newest %d, oldest %d "
                "impulses\n",
                static_cast<unsigned long long>(crowdSize), remembered, newest, oldest);
    Check(remembered == kMaxDepartedWaterContactDepths && newest == 0 && oldest == 1,
          "the tracker remembers at most 256 departed depths and forgets the oldest first");
}

void CheckTeleportAndStaleTracks()
{
    WaterContactTracker tracker;
    WaterRippleDisturbance d[kDisturbanceCapacity];
    const double frame = 1.0 / 60.0;
    double t = kStartSeconds;
    float x = 0.0f;
    for (int i = 0; i < 10; ++i, t += frame, x += static_cast<float>(kRunSpeed * frame))
        tracker.Update(FrameOf({ContactAt(1, x, 0.0f, kWadingDepth)}), t);
    x += 50.0f;
    tracker.Update(FrameOf({ContactAt(1, x, 0.0f, kWadingDepth)}), t);
    const uint32_t jumped = DisturbancesNow(tracker, t, d);
    t += frame;
    x += static_cast<float>(kRunSpeed * frame);
    tracker.Update(FrameOf({ContactAt(1, x, 0.0f, kWadingDepth)}), t);
    const uint32_t resumed = DisturbancesNow(tracker, t, d);
    const float resumedFrom = d[0].from[0];
    Check(jumped == 0 && resumed == 1 && std::fabs(resumedFrom - (x - kRunSpeed * kStepSeconds)) < 0.01f,
          "a jump beyond the unit's speed (teleport, blink) leaves no wake segment and the next frame's wake starts at "
          "the new position");

    t += 0.3;
    tracker.Update(FrameOf({ContactAt(2, 5.0f, 0.0f, kWadingDepth)}), t);
    const uint32_t kept = tracker.Tracks();
    t += 0.3;
    tracker.Update(FrameOf({ContactAt(2, 5.0f, 0.0f, kWadingDepth)}), t);
    const uint32_t dropped = tracker.Tracks();
    Check(kept == 2 && dropped == 1 && tracker.Find(1) == nullptr && tracker.Find(2),
          "a unit missing for 0.3 s keeps its track and one missing for 0.6 s is dropped");

    WaterContactTracker pair;
    t = kStartSeconds;
    x = 0.0f;
    uint32_t count = 0;
    for (int i = 0; i < 10; ++i, t += frame, x += static_cast<float>(kRunSpeed * frame))
    {
        pair.Update(FrameOf({ContactAt(1, x, 0.0f, kWadingDepth), ContactAt(2, 0.0f, 8.0f, kWadingDepth)}), t);
        count = DisturbancesNow(pair, t, d);
    }
    Check(count == 1 && d[0].to[1] == 0.0f && pair.Tracks() == 2,
          "two units are tracked independently: only the moving one leaves a footprint");
}

void CheckWaterContacts()
{
    CheckContactSelection();
    CheckVisibleUnitWalk();
    CheckStationaryUnitStopsEmitting();
    CheckEntryImpulses();
    CheckReturningUnitsKeepTheirSplashDepth();
    CheckTeleportAndStaleTracks();
}
}
