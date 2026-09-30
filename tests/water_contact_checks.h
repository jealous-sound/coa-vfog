#pragma once

namespace water_contact_checks
{
constexpr double kStartSeconds = 100.0;
constexpr float kSurfaceZ = 10.0f;
constexpr float kUnitHeight = 2.0f;
constexpr float kUnitRadius = 0.35f;
constexpr float kWadingDepth = 0.5f;
constexpr float kAnkleDepth = 0.25f;
constexpr float kRunSpeed = 7.0f;
constexpr float kChargeSpeed = 30.0f;
constexpr float kBlinkYards = 20.0f;
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
constexpr uintptr_t kUnitRadiusField = 0x850;
constexpr uintptr_t kUnitHeightField = 0x854;
constexpr size_t kUnitBytes = 0x860;
constexpr size_t kSmallObjectBytes = 0x40;
constexpr size_t kDescriptorBytes = 0x20;
constexpr uintptr_t kDescriptorTypeMask = 0x08;
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
constexpr uint32_t kCappedObjects = 4100;
constexpr uint32_t kDisturbanceCapacity = kMaxWaterRippleDisturbances;
constexpr double kFrameSeconds = 1.0 / 60.0;
constexpr int kFrameRates[] = {30, 37, 60, 144};
constexpr int kReferenceFrameRate = 60;
constexpr double kStandSeconds = 2.0;
constexpr double kRunSeconds = 2.0;
constexpr double kStillSeconds = 3.0;
constexpr float kPathTolerance = 2e-3f;
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
    uint32_t movementFlags = 0;
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

    uintptr_t AddObject(size_t bytes, uint32_t typeMask)
    {
        const uintptr_t object = Allocate(bytes);
        const uintptr_t descriptors = Allocate(kDescriptorBytes);
        Put<uint32_t>(descriptors + kDescriptorTypeMask, typeMask);
        Put<uintptr_t>(object + kObjectDescriptors, descriptors);
        Put<uintptr_t>(object + kListLinkOffset, m_lastLink);
        Put<uintptr_t>(object + kListLinkOffset + sizeof(uintptr_t), End());
        Put<uintptr_t>(m_lastLink + sizeof(uintptr_t), object);
        m_lastLink = object + kListLinkOffset;
        return object;
    }

    uintptr_t AddUnit(const UnitFixture& unit)
    {
        const uintptr_t object = AddObject(kUnitBytes, unit.typeMask);
        Put<uint64_t>(object + kObjectGuid, unit.guid);
        Put<uintptr_t>(object + kUnitMovement, unit.movementInBlock ? object + kUnitMovementBlock : object);
        Put<uint64_t>(object + kUnitTransportGuid, unit.transportGuid);
        std::memcpy(reinterpret_cast<void*>(object + kUnitPosition), unit.position, sizeof(unit.position));
        Put<uint32_t>(object + kUnitMovementFlags, unit.movementFlags);
        Put<float>(object + kUnitRadiusField, unit.radius);
        Put<float>(object + kUnitHeightField, unit.height);
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
    world.AddUnit(wading);
    UnitFixture player = WadingUnit(2, -3.0f, 0.5f);
    player.typeMask = kPlayerTypeMask;
    player.movementFlags = kForwardFlag;
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
    UnitFixture flat = WadingUnit(11, 1.0f, 1.0f);
    flat.height = 0.0f;
    for (const UnitFixture* rejected : {&dry, &above, &gameObject, &detached, &tooDeep, &unloaded, &broken, &flat})
        world.AddUnit(*rejected);
    WaterContactFrame frame;
    const bool captured = engine::CaptureWaterContactsFrom(world.Manager(), centre, frame);
    const WaterContact* swimmer = ContactOf(frame, 1);
    const WaterContact* walker = ContactOf(frame, 2);
    const WaterContact* rider = ContactOf(frame, 3);
    std::printf("     synthetic object manager: captured %d, %u contacts\n", captured, frame.count);
    Check(captured && frame.count == 3 && swimmer && walker && rider && !frame.truncated,
          "the unit walk keeps units and players whose entity has liquid (0x20) crossing the body (0x100) and skips "
          "dry units, units above the surface, game objects, units without the embedded movement block or a loaded "
          "model, units deeper than max(1, 2h) and non-finite or empty sizes");
    Check(swimmer && walker && swimmer->position[0] == wading.position[0] && swimmer->surface == kSurfaceZ &&
              swimmer->radius == kUnitRadius && swimmer->height == kUnitHeight && swimmer->swimming &&
              !walker->swimming && !swimmer->onTransport &&
              std::fabs(WaterContactDepth(*swimmer) - kWadingDepth) < 1e-5f,
          "a captured contact carries the raw position, the liquid surface, the collision radius and height and the "
          "swimming flag (0x200000) of the movement flags");
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

struct StepDisturbances
{
    double seconds = 0.0;
    std::vector<WaterRippleDisturbance> footprints;
    std::vector<WaterRippleDisturbance> impulses;
};

class StepRecorder
{
public:
    void Frame(WaterContactTracker& tracker, const WaterContactFrame& frame, double seconds)
    {
        tracker.Update(frame, seconds);
        while (StepSeconds(m_steps + 1) <= seconds + 1e-9)
        {
            StepDisturbances step;
            step.seconds = StepSeconds(++m_steps);
            WaterRippleDisturbance d[kDisturbanceCapacity];
            const uint32_t count = tracker.TakeDisturbances(step.seconds, d, kDisturbanceCapacity);
            for (uint32_t i = 0; i < count; ++i)
                (d[i].held ? step.footprints : step.impulses).push_back(d[i]);
            m_log.push_back(step);
        }
    }

    const std::vector<StepDisturbances>& Steps() const { return m_log; }

private:
    static double StepSeconds(uint64_t step) { return kStartSeconds + step * kWaterRippleStepSeconds; }

    uint64_t m_steps = 0;
    std::vector<StepDisturbances> m_log;
};

float SegmentLength(const WaterRippleDisturbance& d)
{
    return std::hypot(d.to[0] - d.from[0], d.to[1] - d.from[1]);
}

bool SameFootprint(const WaterRippleDisturbance& a, const WaterRippleDisturbance& b)
{
    return a.from[0] == b.from[0] && a.from[1] == b.from[1] && a.to[0] == b.to[0] && a.to[1] == b.to[1] &&
           a.radius == b.radius && a.amplitude == b.amplitude && a.held == b.held;
}

bool At(const float point[2], float x, float y)
{
    return std::fabs(point[0] - x) <= kPathTolerance && std::fabs(point[1] - y) <= kPathTolerance;
}

void CheckFootprintShape()
{
    Check(ClientRippleStrength(0.9f, kUnitHeight) == 1.0f && Near(ClientRippleStrength(3.0f, kUnitHeight), 0.75f) &&
              Near(ClientRippleStrength(3.99f, kUnitHeight), 0.5025f),
          "the depth strength is 1 down to half of max(1, 2h) and falls to 0.5 at that depth (0x71CDCF-0x71CDF5)");
    const float full = WaterFootprintLevel(1.0f, kUnitHeight);
    std::printf("     footprint of a 0.35 yd, 2 yd unit: radius %.3f yd; level %.3f dry, %.3f ankle-deep, %.3f "
                "shin-deep, %.3f waist-deep, %.3f swimming at 1.5 yd, %.3f at 3 yd\n",
                WaterFootprintRadius(kUnitRadius), WaterFootprintLevel(0.0f, kUnitHeight),
                WaterFootprintLevel(kAnkleDepth, kUnitHeight), WaterFootprintLevel(kWadingDepth, kUnitHeight), full,
                WaterFootprintLevel(1.5f, kUnitHeight), WaterFootprintLevel(3.0f, kUnitHeight));
    Check(Near(WaterFootprintRadius(kUnitRadius), 1.25f * kUnitRadius) && WaterFootprintRadius(0.1f) == 0.3f &&
              WaterFootprintRadius(5.0f) == 6.0f,
          "a footprint's radius is 1.25 collision radii, 0.3 to 6 yd, so its weight falls to a half at about the "
          "collision radius");
    Check(full > 0.0f && WaterFootprintLevel(0.0f, kUnitHeight) == 0.0f &&
              Near(WaterFootprintLevel(kAnkleDepth, kUnitHeight), 0.5f * full) &&
              Near(WaterFootprintLevel(kWadingDepth, kUnitHeight), full) &&
              Near(WaterFootprintLevel(1.5f, kUnitHeight), full) &&
              Near(WaterFootprintLevel(3.0f, kUnitHeight), 0.75f * full),
          "a footprint raises the surface in proportion to the depth up to a quarter of the unit's height and by "
          "the client's depth strength beyond");
    Check(WaterFootprintFade(0.0, 0.0) == 0.0f && Near(WaterFootprintFade(0.5, 0.0), 0.5f) &&
              WaterFootprintFade(1.0, 0.0) == 1.0f && WaterFootprintFade(5.0, -1.0) == 1.0f &&
              Near(WaterFootprintFade(5.0, 0.25), 0.5f) && WaterFootprintFade(5.0, 0.5) == 0.0f,
          "a footprint fades in smoothly over 1 s after it is placed and out over the 0.5 s a missing unit is kept");
    Check(PlausibleWaterWakeStep(kRunSpeed / 60.0f, 1.0 / 60.0) &&
              PlausibleWaterWakeStep(kChargeSpeed / 60.0f, 1.0 / 60.0) &&
              !PlausibleWaterWakeStep(kBlinkYards, 1.0 / 60.0) && PlausibleWaterWakeStep(0.4f, 0.0) &&
              !PlausibleWaterWakeStep(1.0f, 0.0),
          "a move is followed up to 40 yd/s plus 0.5 yd; a longer jump is a teleport");
}

StepRecorder StandRunStop(int framesPerSecond)
{
    WaterContactTracker tracker;
    StepRecorder recorder;
    const double end = kStandSeconds + kRunSeconds + kStillSeconds;
    const int frames = static_cast<int>(std::lround(end * framesPerSecond));
    for (int frame = 0; frame <= frames; ++frame)
    {
        const double elapsed = static_cast<double>(frame) / framesPerSecond;
        const double running = std::clamp(elapsed - kStandSeconds, 0.0, kRunSeconds);
        const WaterContact unit = ContactAt(1, static_cast<float>(kRunSpeed * running), 0.0f, kWadingDepth);
        recorder.Frame(tracker, FrameOf({unit}), kStartSeconds + elapsed);
    }
    return recorder;
}

struct FootprintHistory
{
    int steps = 0;
    int single = 0;
    int moves = 0;
    int changesWhileStill = 0;
    int transitions = 0;
    bool chained = true;
    bool onPath = true;
    float longest = 0.0f;
    float runningLevel = 0.0f;
    float standingLevel = 0.0f;
};

FootprintHistory HistoryOf(const StepRecorder& recorder)
{
    FootprintHistory h;
    const std::vector<StepDisturbances>& steps = recorder.Steps();
    bool wasMoving = false;
    double stillSince = kStartSeconds;
    for (size_t i = 0; i < steps.size(); ++i)
    {
        const StepDisturbances& step = steps[i];
        ++h.steps;
        if (step.footprints.size() != 1 || !step.impulses.empty())
            continue;
        ++h.single;
        const WaterRippleDisturbance& f = step.footprints[0];
        const double elapsed = step.seconds - kStartSeconds;
        const float x = static_cast<float>(kRunSpeed * std::clamp(elapsed - kStandSeconds, 0.0, kRunSeconds));
        h.onPath = h.onPath && At(f.to, x, 0.0f);
        const float length = SegmentLength(f);
        h.longest = std::max(h.longest, length);
        const bool moving = length > kPathTolerance;
        h.moves += moving ? 1 : 0;
        (moving ? h.runningLevel : h.standingLevel) = f.amplitude;
        if (i == 0)
        {
            wasMoving = moving;
            continue;
        }
        const WaterRippleDisturbance& previous = steps[i - 1].footprints.empty() ? f : steps[i - 1].footprints[0];
        h.chained = h.chained && At(f.from, previous.to[0], previous.to[1]);
        h.transitions += moving != wasMoving ? 1 : 0;
        stillSince = moving || !wasMoving ? stillSince : step.seconds;
        const bool settled = elapsed > kWaterFootprintFadeSeconds + kWaterRippleStepSeconds &&
                             step.seconds - stillSince > kWaterWakeRampSeconds + kWaterRippleStepSeconds;
        h.changesWhileStill += !moving && !wasMoving && settled && !SameFootprint(f, previous) ? 1 : 0;
        wasMoving = moving;
    }
    return h;
}

void CheckStandingAndMovingFootprints()
{
    WaterContactTracker tracker;
    StepRecorder standing;
    for (int frame = 0; frame <= static_cast<int>(kStillSeconds * kReferenceFrameRate); ++frame)
        standing.Frame(tracker, FrameOf({ContactAt(1, 2.0f, 3.0f, kWadingDepth)}),
                       kStartSeconds + static_cast<double>(frame) / kReferenceFrameRate);
    const float full = WaterFootprintLevel(kWadingDepth, kUnitHeight);
    const float level = full * WaterFootprintDepthShare(0.0f);
    bool still = !standing.Steps().empty();
    bool growing = true;
    float last = 0.0f;
    for (const StepDisturbances& step : standing.Steps())
    {
        const bool one = step.footprints.size() == 1 && step.impulses.empty();
        still = still && one;
        if (!one)
            continue;
        const WaterRippleDisturbance& f = step.footprints[0];
        const double since = step.seconds - kStartSeconds;
        still = still && At(f.from, 2.0f, 3.0f) && At(f.to, 2.0f, 3.0f) &&
                f.radius == WaterFootprintRadius(kUnitRadius);
        growing = growing && f.amplitude >= last && f.amplitude <= level &&
                  (since < kWaterFootprintFadeSeconds + kWaterRippleStepSeconds || f.amplitude == level);
        last = f.amplitude;
    }
    const long steps = std::lround(kStillSeconds / kWaterRippleStepSeconds);
    Check(still && growing && last == level && standing.Steps().size() == static_cast<size_t>(steps),
          "a unit standing in the water holds one footprint at its position in every 30 Hz step, rising smoothly "
          "over 1 s and unchanged after that");

    const FootprintHistory h = HistoryOf(StandRunStop(kReferenceFrameRate));
    const int runSteps = static_cast<int>(std::lround(kRunSeconds / kWaterRippleStepSeconds));
    std::printf("     stand %.0f s, run %.0f s at %.0f yd/s, stand %.0f s: %d steps, %d with one footprint, %d moving "
                "(longest %.3f yd), %d start or stop, %d changes while standing\n",
                kStandSeconds, kRunSeconds, kRunSpeed, kStillSeconds, h.steps, h.single, h.moves, h.longest,
                h.transitions, h.changesWhileStill);
    Check(h.single == h.steps && h.moves == runSteps && h.transitions == 2 && h.changesWhileStill == 0 && h.chained &&
              h.onPath && std::fabs(h.longest - kRunSpeed * kWaterRippleStepSeconds) <= kPathTolerance,
          "a unit that stands, runs and stops changes its footprint only while it moves and while its depth follows "
          "its speed: each step holds it along the segment from where the last step left it to where the unit is, "
          "so the footprint starts moving once and stops once");
    std::printf("     footprint level standing %.3f, running at %.0f yd/s %.3f, standing again %.3f\n", level,
                kRunSpeed, h.runningLevel, h.standingLevel);
    Check(Near(h.runningLevel, full) && Near(h.standingLevel, level) && Near(WaterFootprintDepthShare(0.0f), 0.2f) &&
              WaterFootprintDepthShare(1.0f) == 1.0f && Near(WaterWakeMotion(2.25f), 0.5f) &&
              WaterWakeMotion(kRunSpeed) == 1.0f && WaterWakeMotion(0.0f) == 0.0f,
          "a standing unit holds a fifth of its footprint's depth and one moving at 4.5 yd/s or faster all of it, "
          "the depth following the speed over 0.3 s");
}

void CheckFootprintsFollowThePathAtAnyFrameRate()
{
    bool same = true;
    for (int framesPerSecond : kFrameRates)
    {
        const FootprintHistory h = HistoryOf(StandRunStop(framesPerSecond));
        std::printf("     %3d fps: %d steps, %d moving, longest %.4f yd, chained %d, on the path %d\n", framesPerSecond,
                    h.steps, h.moves, h.longest, h.chained, h.onPath);
        same = same && h.single == h.steps && h.chained && h.onPath && h.transitions == 2 &&
               h.longest <= kRunSpeed * kWaterRippleStepSeconds + kPathTolerance;
    }
    Check(same, "at 30, 37, 60 and 144 fps every step holds the footprint where the unit is at that step's time, "
                "interpolated between the frames around it, from where the last step left it");
}

void CheckTeleportRestartsTheFootprint()
{
    WaterContactTracker tracker;
    StepRecorder recorder;
    const double jumpAt = 1.0;
    for (int frame = 0; frame <= 2 * kReferenceFrameRate; ++frame)
    {
        const double elapsed = static_cast<double>(frame) / kReferenceFrameRate;
        const float y = elapsed >= jumpAt ? kBlinkYards : 0.0f;
        recorder.Frame(tracker, FrameOf({ContactAt(1, static_cast<float>(kRunSpeed * elapsed), y, kWadingDepth)}),
                       kStartSeconds + elapsed);
    }
    float longest = 0.0f;
    bool restarted = false;
    for (const StepDisturbances& step : recorder.Steps())
        for (const WaterRippleDisturbance& f : step.footprints)
        {
            longest = std::max(longest, SegmentLength(f));
            restarted = restarted || (At(f.from, f.to[0], f.to[1]) && f.to[1] == kBlinkYards && f.amplitude == 0.0f);
        }
    const WaterContactTrack* blinked = tracker.Find(1);

    WaterContactTracker charging;
    StepRecorder charge;
    for (int frame = 0; frame <= kReferenceFrameRate; ++frame)
    {
        const double elapsed = static_cast<double>(frame) / kReferenceFrameRate;
        charge.Frame(charging, FrameOf({ContactAt(2, static_cast<float>(kChargeSpeed * elapsed), 0.0f, kWadingDepth)}),
                     kStartSeconds + elapsed);
    }
    float chargeLongest = 0.0f;
    for (const StepDisturbances& step : charge.Steps())
        for (const WaterRippleDisturbance& f : step.footprints)
            chargeLongest = std::max(chargeLongest, SegmentLength(f));
    std::printf("     running unit blinking %.0f yd: longest footprint %.3f yd, placed %u times; unit charging at %.0f "
                "yd/s: longest %.3f yd, placed %u times\n",
                kBlinkYards, longest, blinked ? blinked->placements : 0, kChargeSpeed, chargeLongest,
                charging.Find(2) ? charging.Find(2)->placements : 0);
    Check(blinked && blinked->placements == 2 && restarted &&
              longest <= kRunSpeed * kWaterRippleStepSeconds + kPathTolerance && charging.Find(2) &&
              charging.Find(2)->placements == 1 &&
              std::fabs(chargeLongest - kChargeSpeed * kWaterRippleStepSeconds) <= kPathTolerance,
          "a unit that jumps farther than 40 yd/s allows in a frame drags no footprint across the jump: its footprint "
          "starts again where it landed and fades in; a unit charging at 30 yd/s is followed step by step");
}

void CheckMissingUnitFadesOut()
{
    WaterContactTracker tracker;
    StepRecorder recorder;
    const int present = 2 * kReferenceFrameRate;
    for (int frame = 0; frame <= 3 * kReferenceFrameRate; ++frame)
    {
        const double elapsed = static_cast<double>(frame) / kReferenceFrameRate;
        const WaterContactFrame frameContacts =
            frame <= present ? FrameOf({ContactAt(1, 4.0f, -1.0f, kWadingDepth)}) : FrameOf({});
        recorder.Frame(tracker, frameContacts, kStartSeconds + elapsed);
    }
    const double lastSeen = kStartSeconds + static_cast<double>(present) / kReferenceFrameRate;
    bool fading = true;
    float previous = WaterFootprintLevel(kWadingDepth, kUnitHeight);
    int afterDrop = 0;
    int held = 0;
    for (const StepDisturbances& step : recorder.Steps())
    {
        if (step.seconds <= lastSeen)
            continue;
        if (step.seconds > lastSeen + kStaleWaterTrackSeconds + kFrameSeconds)
        {
            afterDrop += static_cast<int>(step.footprints.size() + step.impulses.size());
            continue;
        }
        for (const WaterRippleDisturbance& f : step.footprints)
        {
            ++held;
            fading = fading && At(f.to, 4.0f, -1.0f) && f.amplitude <= previous && f.amplitude >= 0.0f;
            previous = f.amplitude;
        }
    }
    std::printf("     unit gone after %.0f s: %d fading footprints, last level %.4f, %d disturbances after its track "
                "was dropped\n",
                static_cast<double>(present) / kReferenceFrameRate, held, previous, afterDrop);
    Check(fading && held > 0 && previous < 1e-3f && afterDrop == 0 && tracker.Tracks() == 0,
          "a unit that is no longer seen keeps its footprint where it was last seen, settling back to the surface "
          "over the 0.5 s its track is kept, and then nothing");
}

void CheckUnitsAreFollowedAlike()
{
    WaterContactTracker tracker;
    StepRecorder recorder;
    for (int frame = 0; frame <= 2 * kReferenceFrameRate; ++frame)
    {
        const double elapsed = static_cast<double>(frame) / kReferenceFrameRate;
        const float x = static_cast<float>(kRunSpeed * elapsed);
        recorder.Frame(tracker, FrameOf({ContactAt(7, x, 8.0f, kWadingDepth), ContactAt(1, x, -8.0f, kWadingDepth),
                                         ContactAt(3, 0.0f, 20.0f, kWadingDepth)}),
                       kStartSeconds + elapsed);
    }
    bool alike = !recorder.Steps().empty();
    for (const StepDisturbances& step : recorder.Steps())
    {
        alike = alike && step.footprints.size() == 3;
        if (step.footprints.size() != 3)
            continue;
        const WaterRippleDisturbance& npc = step.footprints[0];
        const WaterRippleDisturbance& player = step.footprints[1];
        const WaterRippleDisturbance& still = step.footprints[2];
        alike = alike && npc.from[0] == player.from[0] && npc.to[0] == player.to[0] && npc.from[1] == 8.0f &&
                player.from[1] == -8.0f && npc.radius == player.radius && npc.amplitude == player.amplitude &&
                At(still.from, 0.0f, 20.0f) && At(still.to, 0.0f, 20.0f);
    }
    Check(alike && tracker.Tracks() == 3,
          "units are followed independently and alike: two units of the same size and depth running side by side hold "
          "the same footprints at their own positions while a third stands still");
}

StepDisturbances TakeStep(WaterContactTracker& tracker, double seconds)
{
    StepDisturbances step;
    step.seconds = seconds;
    WaterRippleDisturbance d[kDisturbanceCapacity];
    const uint32_t count = tracker.TakeDisturbances(seconds, d, kDisturbanceCapacity);
    for (uint32_t i = 0; i < count; ++i)
        (d[i].held ? step.footprints : step.impulses).push_back(d[i]);
    return step;
}

struct ImpulseLog
{
    int impulses = 0;
    float amplitude = 0.0f;
    float radius = 0.0f;
};

void Descend(WaterContactTracker& tracker, double& t, float depth, bool swimming, ImpulseLog& log)
{
    WaterContact contact = ContactAt(1, 0.0f, 0.0f, depth);
    contact.swimming = swimming;
    tracker.Update(FrameOf({contact}), t);
    for (const WaterRippleDisturbance& impulse : TakeStep(tracker, t).impulses)
    {
        ++log.impulses;
        log.amplitude = impulse.amplitude;
        log.radius = impulse.radius;
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
    std::printf("     entry impulses: %d entering (amplitude %.3f over %.2f yd), %d leaving, %d while swimming\n",
                entering.impulses, entering.amplitude, entering.radius, leaving.impulses, swimming.impulses);
    Check(entering.impulses == 1 && entering.amplitude < 0.0f && leaving.impulses == 0 && swimming.impulses == 0,
          "a unit that is not swimming makes one entry impulse when its depth rises from the shallows across 0.4 of "
          "its height, none when it crosses back on the way out (where the client's 0x730D10 splashes too) and none "
          "while it swims");
    Check(Near(entering.radius, WaterSplashRadius(kUnitRadius)) &&
              WaterSplashRadius(kUnitRadius) > WaterFootprintRadius(kUnitRadius) &&
              Near(WaterEntryImpulse(1.2f, kUnitHeight), entering.amplitude),
          "the entry splash reaches beyond the unit's footprint (three collision radii, 0.75 to 9 yd), so the "
          "footprint's hold does not swallow it");

    WaterContactTracker fresh;
    t = kStartSeconds;
    fresh.Update(FrameOf({ContactAt(1, 0.0f, 0.0f, kDeepDepth)}), t);
    const size_t seeded = TakeStep(fresh, t).impulses.size();
    size_t appeared = 0;
    for (int frame = 0; frame < kPairFrames; ++frame)
    {
        t += kFrameSeconds;
        fresh.Update(FrameOf({ContactAt(1, 0.0f, 0.0f, kDeepDepth), ContactAt(2, 3.0f, 0.0f, kDeepDepth)}), t);
        appeared += TakeStep(fresh, t).impulses.size();
    }
    size_t wadedIn = 0;
    float splashX = 0.0f;
    for (float depth : {kShoreDepth, kWaistDepth})
    {
        t += kFrameSeconds;
        fresh.Update(FrameOf({ContactAt(3, 6.0f, 0.0f, depth)}), t);
        for (const WaterRippleDisturbance& impulse : TakeStep(fresh, t).impulses)
        {
            ++wadedIn;
            splashX = impulse.to[0];
        }
    }
    std::printf("     first seen at 0.75h: %zu impulses at the start, %zu later; first seen at 0.1h, then 0.7h: %zu\n",
                seeded, appeared, wadedIn);
    Check(seeded == 0 && appeared == 0 && wadedIn == 1 && splashX == 6.0f,
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
        for (const WaterRippleDisturbance& impulse : TakeStep(tracker, t).impulses)
            impulses += impulse.amplitude < 0.0f ? 1 : 0;
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

void CheckStaleTracksAndCapacity()
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

    WaterContactTracker crowd;
    t = kStartSeconds;
    WaterContactFrame first;
    WaterContactFrame second;
    for (uint32_t i = 0; i < kMaxWaterContacts; ++i)
    {
        first.contacts[first.count++] = ContactAt(100 + i, static_cast<float>(i), 0.0f, kShoreDepth);
        second.contacts[second.count++] = ContactAt(200 + i, static_cast<float>(i), 5.0f, kWaistDepth);
    }
    crowd.Update(first, t);
    TakeStep(crowd, t);
    t += kFrameSeconds;
    crowd.Update(second, t);
    const StepDisturbances full = TakeStep(crowd, t);
    bool freshFirst = full.footprints.size() == kDisturbanceCapacity;
    for (uint32_t i = 0; freshFirst && i < kMaxWaterContacts; ++i)
        freshFirst = full.footprints[i].to[1] == 5.0f;
    std::printf("     32 units replaced by 32 others: %zu footprints in the step, fresh ones first %d\n",
                full.footprints.size(), freshFirst);
    Check(freshFirst && full.impulses.empty(),
          "a step holds the footprints of the units seen in the last frame first and fills the rest of its 48 slots "
          "with units still fading out");
}

class Wader
{
public:
    void Wade(float depth, double seconds) { Frames(depth, seconds, true); }
    void Leave(double seconds) { Frames(0.0f, seconds, false); }
    uint32_t Splashes() const { return m_splashes; }
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
        WaterContact contact = ContactAt(kGuid, static_cast<float>(kRunSpeed * (m_t - kStartSeconds)), 0.0f, depth);
        m_tracker.Update(present ? FrameOf({contact}) : FrameOf({}), m_t);
        m_splashes += static_cast<uint32_t>(TakeStep(m_tracker, m_t).impulses.size());
    }

    static constexpr uint64_t kGuid = 1;
    WaterContactTracker m_tracker;
    double m_t = kStartSeconds;
    uint32_t m_splashes = 0;
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
    std::printf("     wading in from the shore, then 0.35h <-> 0.45h every %.1f s for %.1f s: %u splashes\n",
                kUnevenFloorHalfPeriod, 2.0 * kUnevenFloorHalfPeriod * cycles, wader.Splashes());
    Check(onShore == 0 && wader.Splashes() == 1,
          "a unit wading in from the shore over a floor that takes its depth back and forth across 0.4 of its height "
          "splashes once on entering");
}

struct Reentry
{
    uint32_t splashes = 0;
    bool dropped = false;
};

Reentry WadeOutAndBack(float outDepth, double sinceSplash)
{
    Wader wader;
    wader.Wade(kShoreDepth, kShoreSeconds);
    wader.Wade(kWaistDepth, 0.5 * sinceSplash);
    wader.Wade(outDepth, 0.5 * sinceSplash);
    wader.Wade(kWaistDepth, kLaterThanSplashInterval);
    return {wader.Splashes(), false};
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
    return {wader.Splashes(), dropped};
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
    Check(later.splashes == 2 && belowRearm.splashes == 2 && aboveRearm.splashes == 1,
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
    CheckFootprintShape();
    CheckStandingAndMovingFootprints();
    CheckFootprintsFollowThePathAtAnyFrameRate();
    CheckTeleportRestartsTheFootprint();
    CheckMissingUnitFadesOut();
    CheckUnitsAreFollowedAlike();
    CheckEntryImpulses();
    CheckUnevenFloorSplashesOnce();
    CheckEntrySplashNeedsRearming();
    CheckReturningUnitsKeepTheirEntryState();
    CheckStaleTracksAndCapacity();
}
}
