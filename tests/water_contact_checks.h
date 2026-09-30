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
constexpr float kClientRippleReferenceSpeed = 2.5f;
constexpr float kClientRippleMaxSpeed = 20.0f;
constexpr float kClientRippleMinSpeed = 1e-4f;
constexpr float kClientMovingRippleMsPerStrength = 250.0f;
constexpr uint32_t kClientIdleRippleMs = 400;
constexpr uint32_t kClientIdleRippleSpreadMs = 50;
constexpr uint32_t kClientMoveFlags = 0xF;
constexpr uint32_t kCappedObjects = 4100;
constexpr uint32_t kDisturbanceCapacity = 32;
constexpr double kFrameSeconds = 1.0 / 60.0;
constexpr double kMsPerSecond = 1000.0;
constexpr int kRingFrameRates[] = {30, 60, 144};
constexpr double kRingRunSeconds = 2.0;
constexpr int kIdleFrameRate = 60;
constexpr double kIdleRunSeconds = 3.0;
constexpr uint32_t kMinIdleRipples = 6;
constexpr uint32_t kMaxIdleRipples = 7;
constexpr uint32_t kStampFrames = 7;
constexpr uint32_t kJitterSamples = 1000;
constexpr uint32_t kJitterStampStep = 89;
constexpr float kMinJitterSpread = 0.15f;
constexpr float kMinRingRadius = 0.25f;
constexpr int kPairFrames = 30;
constexpr uint64_t kCrowdOverflow = 44;
constexpr float kShoreDepth = 0.1f * kUnitHeight;
constexpr float kWaistDepth = 0.7f * kUnitHeight;
constexpr float kDeepDepth = 0.75f * kUnitHeight;
constexpr float kBelowSplashDepth = 0.35f * kUnitHeight;
constexpr float kAboveSplashDepth = 0.45f * kUnitHeight;
constexpr double kShoreSeconds = 0.5;
constexpr double kUnevenFloorHalfPeriod = 0.3;
constexpr double kUnevenFloorSeconds = 10.0;
constexpr float kRearmMargin = 0.02f * kUnitHeight;
constexpr double kShoreStepSeconds = 0.1;
constexpr double kSoonerThanSplashInterval = 0.75 * kMinWaterSplashIntervalSeconds;
constexpr double kLaterThanSplashInterval = 1.25 * kMinWaterSplashIntervalSeconds;

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
              liveFrame.count == 0 && !engine::WaterContactsSupported(),
          "a manager with another list link offset is rejected, no manager (before login) is an empty frame, and the "
          "live capture refuses the harness image and reports itself unsupported");
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

struct ClientRippleClock
{
    uint32_t next = 0;
    uint32_t emitted = 0;

    uint32_t Due(uint32_t nowMs, const WaterContact& contact)
    {
        if (next && static_cast<int32_t>(nowMs - next) < 0)
            return next;
        const float strength = ClientRippleStrength(WaterContactDepth(contact), contact.height);
        next = nowMs + ((contact.movementFlags & kClientMoveFlags) != 0
                            ? MovingInterval(contact.speed, strength)
                            : kClientIdleRippleMs + (nowMs * 7) % kClientIdleRippleSpreadMs);
        ++emitted;
        return next;
    }

    static uint32_t MovingInterval(float speed, float strength)
    {
        const float pace = speed > kClientRippleMinSpeed
                               ? kClientRippleReferenceSpeed / std::min(speed, kClientRippleMaxSpeed)
                               : 1.0f;
        return static_cast<uint32_t>(strength * pace * kClientMovingRippleMsPerStrength);
    }
};

WaterContactFrame FrameOf(std::initializer_list<WaterContact> contacts)
{
    WaterContactFrame frame;
    for (const WaterContact& contact : contacts)
        frame.contacts[frame.count++] = contact;
    return frame;
}

uint32_t DisturbancesNow(WaterContactTracker& tracker, WaterRippleDisturbance* out)
{
    return tracker.TakeDisturbances(out, kDisturbanceCapacity);
}

uint32_t WorldMsAt(double seconds)
{
    return kWorldMs + static_cast<uint32_t>(std::lround((seconds - kStartSeconds) * kMsPerSecond));
}

WaterContact ClockedContact(uint64_t guid, float x, float y, uint32_t movementFlags, double seconds,
                            ClientRippleClock& clock)
{
    WaterContact contact = ContactAt(guid, x, y, kWadingDepth);
    contact.movementFlags = movementFlags;
    contact.speed = movementFlags ? kRunSpeed : 0.0f;
    contact.nextRippleMs = clock.Due(WorldMsAt(seconds), contact);
    return contact;
}

bool SameDisturbance(const WaterRippleDisturbance& a, const WaterRippleDisturbance& b)
{
    return a.from[0] == b.from[0] && a.from[1] == b.from[1] && a.to[0] == b.to[0] && a.to[1] == b.to[1] &&
           a.radius == b.radius && a.amplitude == b.amplitude;
}

WaterRippleDisturbance ExpectedRing(const WaterContact& contact)
{
    const float strength = ClientRippleStrength(WaterContactDepth(contact), contact.height);
    const ClientRipple ripple = ClientRippleOf(ClientRippleKindOf(contact.movementFlags), contact.scale, strength,
                                               ClientRippleSizeJitter(contact.guid, contact.nextRippleMs));
    return WaterRingImpulse(contact.position, ripple);
}

struct RingRun
{
    uint32_t advances = 0;
    uint32_t rings = 0;
    bool matched = true;
};

RingRun RunWithClientClock(int framesPerSecond, double seconds, uint32_t movementFlags)
{
    WaterContactTracker tracker;
    ClientRippleClock clock;
    RingRun run;
    const int frames = static_cast<int>(seconds * framesPerSecond);
    for (int frame = 0; frame <= frames; ++frame)
    {
        const double t = kStartSeconds + static_cast<double>(frame) / framesPerSecond;
        const float x = movementFlags ? static_cast<float>(kRunSpeed * (t - kStartSeconds)) : 0.0f;
        const uint32_t emittedBefore = clock.emitted;
        const WaterContact contact = ClockedContact(1, x, 0.0f, movementFlags, t, clock);
        tracker.Update(FrameOf({contact}), t);
        WaterRippleDisturbance d[kDisturbanceCapacity];
        const uint32_t count = DisturbancesNow(tracker, d);
        const bool advanced = frame > 0 && clock.emitted != emittedBefore;
        run.matched = run.matched && count == (advanced ? 1u : 0u) &&
                      (count == 0 || SameDisturbance(d[0], ExpectedRing(contact)));
        run.rings += count;
    }
    run.advances = clock.emitted - 1;
    return run;
}

uint32_t RingsForStamps(std::initializer_list<uint32_t> stamps, uint32_t* rings)
{
    WaterContactTracker tracker;
    WaterRippleDisturbance d[kDisturbanceCapacity];
    double t = kStartSeconds;
    uint32_t frame = 0;
    for (uint32_t stamp : stamps)
    {
        WaterContact contact = ContactAt(1, static_cast<float>(frame), 0.0f, kWadingDepth);
        contact.movementFlags = kForwardFlag;
        contact.nextRippleMs = stamp;
        tracker.Update(FrameOf({contact}), t);
        rings[frame++] = DisturbancesNow(tracker, d);
        t += kFrameSeconds;
    }
    return frame;
}

void CheckOneRingPerClientRipple()
{
    bool same = true;
    for (int framesPerSecond : kRingFrameRates)
    {
        const RingRun run = RunWithClientClock(framesPerSecond, kRingRunSeconds, kForwardFlag);
        std::printf("     running unit at %3d fps: %u client ripples after the first frame, %u rings\n",
                    framesPerSecond, run.advances, run.rings);
        same = same && run.matched && run.rings == run.advances && run.advances > 0;
    }
    Check(same, "at 30, 60 and 144 fps a running unit makes one ring impulse in each frame its client ripple time "
                "(+0xA58) advances, at its position in that frame, and none in the other frames or the first");

    const RingRun idle = RunWithClientClock(kIdleFrameRate, kIdleRunSeconds, 0);
    std::printf("     standing unit: %u client ripples in %.0f s, %u rings\n", idle.advances, kIdleRunSeconds,
                idle.rings);
    Check(idle.matched && idle.rings == idle.advances && idle.advances >= kMinIdleRipples &&
              idle.advances <= kMaxIdleRipples,
          "a unit standing in the water rings once per client idle ripple (every 400 to 449 ms), with the idle "
          "ripple's size and alpha");

    uint32_t rings[kStampFrames] = {};
    RingsForStamps({kWorldMs, kWorldMs, kWorldMs, 0, 0, kWorldMs + kClientIdleRippleMs, kWorldMs}, rings);
    std::printf("     client ripple times t, t, t, 0, 0, t + 400, t: rings %u %u %u %u %u %u %u\n", rings[0], rings[1],
                rings[2], rings[3], rings[4], rings[5], rings[6]);
    Check(rings[0] == 0 && rings[1] == 0 && rings[2] == 0 && rings[3] == 0 && rings[4] == 0 && rings[5] == 1 &&
              rings[6] == 1,
          "a moving unit whose client ripple time does not change, or is cleared to 0 without an emission, makes no "
          "ring; every change to another non-zero time makes one");
}

void CheckClientRippleMapping()
{
    const bool kinds = ClientRippleKindOf(0x1) == ClientRippleKind::Moving &&
                       ClientRippleKindOf(0x2) == ClientRippleKind::Moving &&
                       ClientRippleKindOf(0x4) == ClientRippleKind::Moving &&
                       ClientRippleKindOf(0x8) == ClientRippleKind::Moving &&
                       ClientRippleKindOf(0x11) == ClientRippleKind::Moving &&
                       ClientRippleKindOf(0x10) == ClientRippleKind::Turning &&
                       ClientRippleKindOf(0x20) == ClientRippleKind::Turning &&
                       ClientRippleKindOf(0) == ClientRippleKind::Idle &&
                       ClientRippleKindOf(kSwimmingFlag) == ClientRippleKind::Idle;
    Check(kinds, "movement flags 0xF make a moving ripple, 0x30 without them a turning ripple and anything else an "
                 "idle ripple, as at 0x71CC0B");

    float lowest = 2.0f;
    float highest = 0.0f;
    bool repeatable = true;
    for (uint32_t i = 1; i <= kJitterSamples; ++i)
    {
        const uint32_t stamp = kWorldMs + i * kJitterStampStep;
        const float jitter = ClientRippleSizeJitter(i, stamp);
        repeatable = repeatable && jitter == ClientRippleSizeJitter(i, stamp);
        lowest = std::min(lowest, jitter);
        highest = std::max(highest, jitter);
    }
    std::printf("     size jitter over %u ripples: %.4f to %.4f\n", kJitterSamples, lowest, highest);
    Check(repeatable && lowest >= 0.9f && highest <= 1.1f && highest - lowest > kMinJitterSpread,
          "the ripple size jitter spans the client's U(0.9, 1.1) and repeats for the same unit and ripple time");

    const ClientRipple moving = ClientRippleOf(ClientRippleKind::Moving, 1.0f, 1.0f, 1.0f);
    const ClientRipple turning = ClientRippleOf(ClientRippleKind::Turning, 1.0f, 1.0f, 1.0f);
    const ClientRipple idle = ClientRippleOf(ClientRippleKind::Idle, 1.0f, 1.0f, 1.0f);
    const ClientRipple huge = ClientRippleOf(ClientRippleKind::Moving, 6.0f, 1.0f, 1.1f);
    const ClientRipple smallest = ClientRippleOf(ClientRippleKind::Moving, 0.5f, 1.0f, 0.9f);
    const ClientRipple deep = ClientRippleOf(ClientRippleKind::Moving, 3.0f, 0.5f, 1.0f);
    const bool sizes = Near(moving.size, 1.0f / 3.0f) && Near(turning.size, moving.size) &&
                       Near(idle.size, 0.6f * moving.size) && Near(huge.size, 5.0f / 3.0f) &&
                       Near(smallest.size, 1.0f / 3.0f) && Near(deep.size, 0.5f);
    const bool alphas =
        moving.alpha == 1.0f && turning.alpha == 1.0f && Near(idle.alpha, 0.8f) && deep.alpha == 0.5f;
    Check(sizes && alphas, "a ripple's size is clamp(scale / 3 * jitter, 1/3, 5/3) times the depth strength, 0.6 of "
                           "that when idle, and its alpha is the strength, 0.8 of it when idle (0x71CD11-0x71CE3A)");
    Check(ClientRippleStrength(0.9f, kUnitHeight) == 1.0f && Near(ClientRippleStrength(3.0f, kUnitHeight), 0.75f) &&
              Near(ClientRippleStrength(3.99f, kUnitHeight), 0.5025f),
          "the depth strength is 1 down to half of max(1, 2h) and falls to 0.5 at that depth (0x71CDCF-0x71CDF5)");

    const float at[2] = {3.0f, -2.0f};
    const WaterRippleDisturbance movingRing = WaterRingImpulse(at, moving);
    const WaterRippleDisturbance idleRing = WaterRingImpulse(at, idle);
    const WaterRippleDisturbance deepRing = WaterRingImpulse(at, deep);
    const WaterRippleDisturbance hugeRing = WaterRingImpulse(at, huge);
    std::printf("     rings: moving r %.3f yd a %.2f, idle r %.3f a %.2f, half strength r %.3f a %.2f, largest r "
                "%.3f\n",
                movingRing.radius, movingRing.amplitude, idleRing.radius, idleRing.amplitude, deepRing.radius,
                deepRing.amplitude, hugeRing.radius);
    Check(movingRing.from[0] == at[0] && movingRing.to[0] == at[0] && movingRing.from[1] == at[1] &&
              movingRing.to[1] == at[1] && Near(movingRing.radius, moving.size) && Near(hugeRing.radius, huge.size) &&
              idleRing.radius == kMinRingRadius && movingRing.amplitude < 0.0f &&
              Near(idleRing.amplitude, 0.8f * movingRing.amplitude) &&
              Near(deepRing.amplitude, 0.5f * movingRing.amplitude),
          "a ring impulse is a point stamp at the ripple's position whose radius is the ripple's size (at least two "
          "0.125 yd texels) and whose depression is proportional to its alpha");

    WaterContactTracker tracker;
    WaterContact large = ContactAt(9, 1.0f, 2.0f, 3.0f);
    large.scale = kLargeScale;
    large.nextRippleMs = kWorldMs;
    tracker.Update(FrameOf({large}), kStartSeconds);
    large.nextRippleMs = kWorldMs + kClientIdleRippleMs;
    tracker.Update(FrameOf({large}), kStartSeconds + kFrameSeconds);
    WaterRippleDisturbance d[kDisturbanceCapacity];
    const uint32_t count = DisturbancesNow(tracker, d);
    const WaterRippleDisturbance expected = WaterRingImpulse(
        large.position, ClientRippleOf(ClientRippleKind::Idle, kLargeScale, ClientRippleStrength(3.0f, kUnitHeight),
                                       ClientRippleSizeJitter(9, kWorldMs + kClientIdleRippleMs)));
    Check(count == 1 && SameDisturbance(d[0], expected) && tracker.Find(9) && tracker.Find(9)->rings == 1,
          "the tracker queues the ring of the unit's kind, scale, depth strength and jitter at its new client ripple "
          "time");
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
    const uint32_t count = DisturbancesNow(tracker, d);
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
    Check(entering.impulses == 1 && entering.amplitude < 0.0f && leaving.impulses == 0 && swimming.impulses == 0,
          "a unit that is not swimming makes one entry impulse when its depth rises from the shallows across 0.4 of "
          "its height, none when it crosses back on the way out (where the client's 0x730D10 splashes too) and none "
          "while it swims");

    WaterContactTracker fresh;
    t = kStartSeconds;
    fresh.Update(FrameOf({ContactAt(1, 0.0f, 0.0f, kDeepDepth)}), t);
    WaterRippleDisturbance d[kDisturbanceCapacity];
    const uint32_t seeded = DisturbancesNow(fresh, d);
    uint32_t appeared = 0;
    for (int frame = 0; frame < kPairFrames; ++frame)
    {
        t += kFrameSeconds;
        fresh.Update(FrameOf({ContactAt(1, 0.0f, 0.0f, kDeepDepth), ContactAt(2, 3.0f, 0.0f, kDeepDepth)}), t);
        appeared += DisturbancesNow(fresh, d);
    }
    uint32_t wadedIn = 0;
    for (float depth : {kShoreDepth, kWaistDepth})
    {
        t += kFrameSeconds;
        fresh.Update(FrameOf({ContactAt(3, 6.0f, 0.0f, depth)}), t);
        wadedIn += DisturbancesNow(fresh, d);
    }
    std::printf("     first seen at 0.75h: %u impulses at the start, %u later; first seen at 0.1h, then 0.7h: %u\n",
                seeded, appeared, wadedIn);
    Check(seeded == 0 && appeared == 0 && wadedIn == 1 && d[0].to[0] == 6.0f,
          "a unit first seen deeper than 0.4 of its height, when the tracker starts or later, makes no impulse then "
          "or while it stays there; a unit first seen in the shallows makes one when it wades in (the client, from "
          "its zeroed +0x784, also splashes a unit created deep)");
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
        const uint32_t count = DisturbancesNow(tracker, d);
        for (uint32_t k = 0; k < count; ++k)
            impulses += d[k].amplitude < 0.0f ? 1 : 0;
    }
    return impulses;
}

void CheckReturningUnitsKeepTheirEntryState()
{
    constexpr float kShore = kShoreDepth;
    constexpr float kWaist = kWaistDepth;
    constexpr float kSwim = 0.8f * kUnitHeight;
    constexpr float kStandUp = kDeepDepth;
    constexpr float kDeep = 1.5f * kUnitHeight;
    constexpr int kDiveFrames = 40;
    constexpr int kForgottenFrames = 31 * 60;
    constexpr int kRememberedFrames = 20 * 60;
    WaterContactTracker diver;
    double t = kStartSeconds;
    ImpulsesOver(diver, t, 1, {kShore, false, true, 2});
    const int wadeIn = ImpulsesOver(diver, t, 1, {kWaist, false, true, 10});
    const int swim = ImpulsesOver(diver, t, 1, {kSwim, true, true, 10});
    const int dive = ImpulsesOver(diver, t, 1, {kDeep, true, false, kDiveFrames});
    const bool dropped = diver.Find(1) == nullptr && diver.RememberedEntries() == 1;
    const int resurface = ImpulsesOver(diver, t, 1, {kSwim, true, true, 10});
    const int standUp = ImpulsesOver(diver, t, 1, {kStandUp, false, true, 10});
    const int wadeOut = ImpulsesOver(diver, t, 1, {kShore, false, true, 10});
    std::printf("     dive and wade out: %d wading in, %d swimming, %d diving (track dropped %d), %d resurfacing, %d "
                "standing up at 0.75h, %d wading out\n",
                wadeIn, swim, dive, dropped, resurface, standUp, wadeOut);
    Check(wadeIn == 1 && swim == 0 && dive == 0 && dropped && resurface == 0 && standUp == 0 && wadeOut == 0,
          "a unit whose track was dropped while it dived keeps its entry state, so standing up at 0.75 of its height "
          "makes no second entry impulse, and wading out makes none");

    WaterContactTracker arrivals;
    t = kStartSeconds;
    const int appearedSwimming = ImpulsesOver(arrivals, t, 2, {kSwim, true, true, 10});
    const int arrivingSwimmer = appearedSwimming + ImpulsesOver(arrivals, t, 2, {kStandUp, false, true, 10});
    ImpulsesOver(arrivals, t, 1, {kShore, false, true, 2});
    const int away = ImpulsesOver(arrivals, t, 1, {kWaist, false, false, kForgottenFrames});
    const uint32_t rememberedAfterAway = arrivals.RememberedEntries();
    const int forgotten = away + ImpulsesOver(arrivals, t, 1, {kStandUp, false, true, 10});
    ImpulsesOver(arrivals, t, 3, {kShore, false, true, 2});
    const int remembered = ImpulsesOver(arrivals, t, 3, {kWaist, false, false, kRememberedFrames}) +
                           ImpulsesOver(arrivals, t, 3, {kStandUp, false, true, 10});
    std::printf("     unit first seen swimming then standing at 0.75h: %d impulses; unit last seen at 0.1h back at "
                "0.75h after 31 s: %d (%u entries remembered), after 20 s: %d\n",
                arrivingSwimmer, forgotten, rememberedAfterAway, remembered);
    Check(arrivingSwimmer == 0 && forgotten == 0 && rememberedAfterAway == 0 && remembered == 1,
          "a unit first seen while swimming makes no impulse when it stands up; a unit that left from the shallows "
          "splashes when it is back at 0.75 of its height within 30 s, as if its track had been kept, and after 30 s "
          "its entry state is forgotten and it counts as new, first seen deep");

    WaterContactTracker crowd;
    t = kStartSeconds;
    const uint64_t crowdSize = kMaxDepartedWaterEntries + kCrowdOverflow;
    for (uint64_t guid = 1; guid <= crowdSize; ++guid)
        ImpulsesOver(crowd, t, guid, {kShore, false, true, 1});
    ImpulsesOver(crowd, t, 0, {kShore, false, false, kDiveFrames});
    const uint32_t rememberedCrowd = crowd.RememberedEntries();
    const int newest = ImpulsesOver(crowd, t, crowdSize, {kStandUp, false, true, 1});
    const int oldest = ImpulsesOver(crowd, t, 1, {kStandUp, false, true, 1});
    std::printf("     %llu units passing by in the shallows: %u entries remembered; back at 0.75h: newest %d, oldest "
                "%d impulses\n",
                static_cast<unsigned long long>(crowdSize), rememberedCrowd, newest, oldest);
    Check(rememberedCrowd == kMaxDepartedWaterEntries && newest == 1 && oldest == 0,
          "the tracker remembers at most 256 departed entry states and forgets the oldest first");
}

void CheckSplashTakesTheClientRippleOfItsFrame()
{
    WaterContactTracker tracker;
    ClientRippleClock clock;
    double t = kStartSeconds;
    uint32_t disturbances = 0;
    uint32_t splashes = 0;
    uint32_t rings = 0;
    const float wader = WaterSplashRadius(kUnitRadius);
    for (float depth : {0.2f, 0.5f, 1.1f, 1.2f})
    {
        WaterContact contact = ContactAt(1, 0.0f, 0.0f, depth);
        contact.movementFlags = kForwardFlag;
        contact.nextRippleMs = clock.Due(WorldMsAt(t), contact);
        clock.next = 0;
        tracker.Update(FrameOf({contact}), t);
        WaterRippleDisturbance d[kDisturbanceCapacity];
        const uint32_t count = DisturbancesNow(tracker, d);
        for (uint32_t i = 0; i < count; ++i)
        {
            ++disturbances;
            splashes += d[i].radius == wader ? 1 : 0;
            rings += d[i].radius == wader ? 0 : 1;
        }
        t += kFrameSeconds;
    }
    std::printf("     wading in with a client ripple every frame: %u disturbances, %u splash, %u rings\n", disturbances,
                splashes, rings);
    Check(splashes == 1 && rings == 2 && disturbances == 3,
          "the frame whose depth crosses 0.4 of the height makes one splash and no ring for the client ripple it "
          "emitted there (the 0xC9 splash of 0x730E42), and the frames around it one ring each");
}

void CheckStaleTracksAndIndependentUnits()
{
    WaterContactTracker tracker;
    double t = kStartSeconds;
    tracker.Update(FrameOf({ContactAt(1, 0.0f, 0.0f, kWadingDepth)}), t);
    t += 0.3;
    tracker.Update(FrameOf({ContactAt(2, 5.0f, 0.0f, kWadingDepth)}), t);
    const uint32_t kept = tracker.Tracks();
    t += 0.3;
    tracker.Update(FrameOf({ContactAt(2, 5.0f, 0.0f, kWadingDepth)}), t);
    const uint32_t dropped = tracker.Tracks();
    Check(kept == 2 && dropped == 1 && tracker.Find(1) == nullptr && tracker.Find(2),
          "a unit missing for 0.3 s keeps its track and one missing for 0.6 s is dropped");

    WaterContactTracker pair;
    ClientRippleClock running;
    WaterRippleDisturbance d[kDisturbanceCapacity];
    t = kStartSeconds;
    uint32_t fromRunner = 0;
    uint32_t fromStill = 0;
    for (int i = 0; i < kPairFrames; ++i, t += kFrameSeconds)
    {
        const float x = static_cast<float>(kRunSpeed * (t - kStartSeconds));
        WaterContact still = ContactAt(2, 0.0f, 8.0f, kWadingDepth);
        still.nextRippleMs = kWorldMs;
        pair.Update(FrameOf({ClockedContact(1, x, 0.0f, kForwardFlag, t, running), still}), t);
        const uint32_t count = DisturbancesNow(pair, d);
        for (uint32_t k = 0; k < count; ++k)
            (d[k].to[1] == 0.0f ? fromRunner : fromStill) += 1;
    }
    Check(fromRunner == running.emitted - 1 && fromRunner > 0 && fromStill == 0 && pair.Tracks() == 2,
          "two units are tracked independently: only the one whose client ripple time advances rings");
}

class Wader
{
public:
    void Wade(float depth, double seconds) { Frames(depth, seconds, true); }
    void Leave(double seconds) { Frames(0.0f, seconds, false); }
    uint32_t Splashes() const { return m_splashes; }
    uint32_t Rings() const { return m_rings; }
    uint32_t Emissions() const { return m_clock.emitted; }
    bool RingsAtClientCadence() const { return m_ringsAtCadence; }
    bool Tracked() const { return m_tracker.Find(kGuid) != nullptr; }

private:
    void Frames(float depth, double seconds, bool present)
    {
        const long frames = std::lround(seconds / kFrameSeconds);
        for (long i = 0; i < frames; ++i)
            Step(depth, present);
    }

    void Step(float depth, bool present)
    {
        m_t += kFrameSeconds;
        const bool tracked = m_tracker.Find(kGuid) != nullptr;
        const uint32_t emittedBefore = m_clock.emitted;
        WaterContact contact = ContactAt(kGuid, static_cast<float>(kRunSpeed * (m_t - kStartSeconds)), 0.0f, depth);
        contact.movementFlags = kForwardFlag;
        if (present)
            contact.nextRippleMs = m_clock.Due(WorldMsAt(m_t), contact);
        m_tracker.Update(present ? FrameOf({contact}) : FrameOf({}), m_t);
        WaterRippleDisturbance d[kDisturbanceCapacity];
        const uint32_t count = DisturbancesNow(m_tracker, d);
        uint32_t splashes = 0;
        for (uint32_t i = 0; i < count; ++i)
            splashes += d[i].radius == WaterSplashRadius(kUnitRadius) ? 1 : 0;
        const bool emitted = present && tracked && m_clock.emitted != emittedBefore;
        const uint32_t expectedRings = emitted && splashes == 0 ? 1 : 0;
        m_ringsAtCadence = m_ringsAtCadence && splashes <= 1 && count - splashes == expectedRings;
        m_splashes += splashes;
        m_rings += count - splashes;
    }

    static constexpr uint64_t kGuid = 1;
    WaterContactTracker m_tracker;
    ClientRippleClock m_clock;
    double m_t = kStartSeconds;
    uint32_t m_splashes = 0;
    uint32_t m_rings = 0;
    bool m_ringsAtCadence = true;
};

void CheckUnevenFloorSplashesOnce()
{
    Wader wader;
    wader.Wade(kShoreDepth, kShoreSeconds);
    const uint32_t onShore = wader.Splashes();
    const long cycles = std::lround(kUnevenFloorSeconds / (2.0 * kUnevenFloorHalfPeriod));
    for (long cycle = 0; cycle < cycles; ++cycle)
    {
        wader.Wade(kAboveSplashDepth, kUnevenFloorHalfPeriod);
        wader.Wade(kBelowSplashDepth, kUnevenFloorHalfPeriod);
    }
    std::printf("     wading in from the shore, then 0.35h <-> 0.45h every %.1f s for %.1f s: %u splashes, %u rings for "
                "%u client ripples\n",
                kUnevenFloorHalfPeriod, 2.0 * kUnevenFloorHalfPeriod * cycles, wader.Splashes(), wader.Rings(),
                wader.Emissions());
    Check(onShore == 0 && wader.Splashes() == 1 && wader.RingsAtClientCadence(),
          "a unit wading in from the shore over a floor that takes its depth back and forth across 0.4 of its height "
          "splashes once on entering; its wake keeps one ring per client ripple in every other frame");
}

struct Reentry
{
    uint32_t splashes = 0;
    bool ringsAtCadence = false;
    bool dropped = false;
};

Reentry WadeOutAndBack(float outDepth, double sinceSplash)
{
    Wader wader;
    wader.Wade(kShoreDepth, kShoreSeconds);
    wader.Wade(kWaistDepth, 0.5 * sinceSplash);
    wader.Wade(outDepth, 0.5 * sinceSplash);
    wader.Wade(kWaistDepth, kLaterThanSplashInterval);
    return {wader.Splashes(), wader.RingsAtClientCadence(), false};
}

Reentry StepAshoreAndBack(double sinceSplash)
{
    Wader wader;
    wader.Wade(kShoreDepth, kShoreSeconds);
    wader.Wade(kWaistDepth, 0.25 * sinceSplash);
    wader.Wade(kShoreDepth, 0.25 * sinceSplash);
    wader.Leave(0.5 * sinceSplash - kShoreStepSeconds);
    const bool dropped = !wader.Tracked();
    wader.Wade(kShoreDepth, kShoreStepSeconds);
    wader.Wade(kWaistDepth, kLaterThanSplashInterval);
    return {wader.Splashes(), wader.RingsAtClientCadence(), dropped};
}

void CheckEntrySplashNeedsRearming()
{
    const float rearmDepth = kWaterSplashRearmDepthPerHeight * kUnitHeight;
    const Reentry later = WadeOutAndBack(kShoreDepth, kLaterThanSplashInterval);
    const Reentry belowRearm = WadeOutAndBack(rearmDepth - kRearmMargin, kLaterThanSplashInterval);
    const Reentry aboveRearm = WadeOutAndBack(rearmDepth + kRearmMargin, kLaterThanSplashInterval);
    const Reentry sooner = WadeOutAndBack(kShoreDepth, kSoonerThanSplashInterval);
    const Reentry ashoreLater = StepAshoreAndBack(kLaterThanSplashInterval);
    const Reentry ashoreSooner = StepAshoreAndBack(kSoonerThanSplashInterval);
    std::printf("     splash, out to 0.1h / 0.13h / 0.17h and back in %.1f s later: %u / %u / %u splashes; back in "
                "%.1f s later: %u\n",
                kLaterThanSplashInterval, later.splashes, belowRearm.splashes, aboveRearm.splashes,
                kSoonerThanSplashInterval, sooner.splashes);
    std::printf("     splash, ashore (track dropped %d / %d), back through the shallows %.1f s / %.1f s later: %u / %u "
                "splashes\n",
                ashoreLater.dropped, ashoreSooner.dropped, kLaterThanSplashInterval, kSoonerThanSplashInterval,
                ashoreLater.splashes, ashoreSooner.splashes);
    const bool cadence = later.ringsAtCadence && belowRearm.ringsAtCadence && aboveRearm.ringsAtCadence &&
                         sooner.ringsAtCadence && ashoreLater.ringsAtCadence && ashoreSooner.ringsAtCadence;
    Check(later.splashes == 2 && belowRearm.splashes == 2 && aboveRearm.splashes == 1 && cadence,
          "a unit that splashed, waded out below 0.15 of its height and back in 2.5 s later splashes again, while one "
          "that only came up to 0.17 of its height does not; wading out never splashes");
    Check(sooner.splashes == 1 && ashoreSooner.splashes == 1 && ashoreSooner.dropped && ashoreLater.splashes == 2 &&
              ashoreLater.dropped,
          "a unit back in within 2 s of its splash, from the shallows or from the shore after its track was dropped, "
          "makes no second splash then or while it stays in; back in after 2 s it does");
}

void CheckWaterContacts()
{
    CheckContactSelection();
    CheckVisibleUnitWalk();
    CheckClientRippleMapping();
    CheckOneRingPerClientRipple();
    CheckEntryImpulses();
    CheckUnevenFloorSplashesOnce();
    CheckEntrySplashNeedsRearming();
    CheckSplashTakesTheClientRippleOfItsFrame();
    CheckReturningUnitsKeepTheirEntryState();
    CheckStaleTracksAndIndependentUnits();
}
}
