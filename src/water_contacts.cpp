#include "water_contacts.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kClientSplashDepthPerHeight = 0.4f;
constexpr float kClientFullStrength = 1.0f;
constexpr float kClientDeepestStrength = 0.5f;
constexpr float kFootprintRadiusPerCollisionRadius = 1.25f;
constexpr float kMinFootprintRadius = 0.3f;
constexpr float kMaxFootprintRadius = 6.0f;
constexpr float kFullImmersionDepthPerHeight = 0.25f;
constexpr float kFullyImmersedFootprintLevel = 3.0f;
constexpr float kStandingFootprintShare = 0.2f;
constexpr float kFullWakeSpeed = 4.5f;
constexpr float kSplashRadiusPerCollisionRadius = 3.0f;
constexpr float kMinSplashRadius = 0.75f;
constexpr float kMaxSplashRadius = 9.0f;
constexpr float kEntryImpulsePerStrength = -1.5f;
constexpr double kDepartedEntrySeconds = 30.0;

bool TakeEntrySplash(WaterEntryState& entry, const WaterContact& contact, double seconds)
{
    if (contact.swimming)
    {
        entry.armed = false;
        return false;
    }
    const float depth = WaterContactDepth(contact);
    if (depth <= kWaterSplashRearmDepthPerHeight * contact.height)
        entry.armed = true;
    if (!entry.armed || depth <= kClientSplashDepthPerHeight * contact.height)
        return false;
    entry.armed = false;
    if (seconds - entry.splashedAt < kMinWaterSplashIntervalSeconds)
        return false;
    entry.splashedAt = seconds;
    return true;
}

double SmoothUnitStep(double x)
{
    const double t = std::clamp(x, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

float GroundDistance(const float a[3], const float b[3])
{
    return std::hypot(a[0] - b[0], a[1] - b[1]);
}

void PlaceFootprint(WaterContactTrack& track, const float position[3], double seconds)
{
    WaterFootprint& footprint = track.footprint;
    for (int axis = 0; axis < 2; ++axis)
        footprint.from[axis] = footprint.stamped[axis] = position[axis];
    footprint.fromSeconds = seconds;
    footprint.placedAt = seconds;
    footprint.motion = 0.0f;
    footprint.steppedAt = -std::numeric_limits<double>::infinity();
    ++track.placements;
}

void FollowMotion(WaterFootprint& footprint, float travelled, double stepSeconds)
{
    const double elapsed = stepSeconds - footprint.steppedAt;
    footprint.steppedAt = stepSeconds;
    if (!(elapsed > 0.0) || !std::isfinite(elapsed))
        return;
    const float ramp = static_cast<float>(elapsed / kWaterWakeRampSeconds);
    const float target = WaterWakeMotion(static_cast<float>(travelled / elapsed));
    footprint.motion += std::clamp(target - footprint.motion, -ramp, ramp);
}

void MoveFootprint(WaterContactTrack& track)
{
    WaterFootprint& footprint = track.footprint;
    for (int axis = 0; axis < 2; ++axis)
        footprint.from[axis] = track.position[axis];
    footprint.fromSeconds = track.seenAt;
}

float PathFraction(const WaterContactTrack& track, double stepSeconds)
{
    const double span = track.seenAt - track.footprint.fromSeconds;
    if (!(span > 0.0))
        return 1.0f;
    return static_cast<float>(std::clamp((stepSeconds - track.footprint.fromSeconds) / span, 0.0, 1.0));
}

WaterRippleDisturbance FootprintStep(WaterContactTrack& track, double stepSeconds)
{
    WaterFootprint& footprint = track.footprint;
    const float along = PathFraction(track, stepSeconds);
    WaterRippleDisturbance step;
    for (int axis = 0; axis < 2; ++axis)
    {
        step.from[axis] = footprint.stamped[axis];
        step.to[axis] = footprint.from[axis] + (track.position[axis] - footprint.from[axis]) * along;
        footprint.stamped[axis] = step.to[axis];
    }
    FollowMotion(footprint, std::hypot(step.to[0] - step.from[0], step.to[1] - step.from[1]), stepSeconds);
    step.radius = footprint.radius;
    step.amplitude = footprint.level *
                     WaterFootprintFade(stepSeconds - footprint.placedAt, stepSeconds - track.seenAt) *
                     WaterFootprintDepthShare(footprint.motion);
    step.held = true;
    return step;
}

WaterRippleDisturbance SplashOf(const WaterContactTrack& track)
{
    WaterRippleDisturbance splash;
    for (int axis = 0; axis < 2; ++axis)
        splash.from[axis] = splash.to[axis] = track.position[axis];
    splash.radius = track.splashRadius;
    splash.amplitude = track.pendingSplash;
    return splash;
}
}

float ClientRippleStrength(float depth, float height)
{
    const float limit = ClientRippleDepthLimit(height);
    if (depth <= 0.5f * limit)
        return kClientFullStrength;
    return std::max(kClientDeepestStrength, kClientDeepestStrength + (limit - depth) / limit);
}

float WaterFootprintRadius(float collisionRadius)
{
    return std::clamp(kFootprintRadiusPerCollisionRadius * collisionRadius, kMinFootprintRadius, kMaxFootprintRadius);
}

float WaterFootprintLevel(float depth, float height)
{
    const float immersion = std::clamp(depth / (kFullImmersionDepthPerHeight * height), 0.0f, 1.0f);
    return kFullyImmersedFootprintLevel * immersion * ClientRippleStrength(depth, height);
}

float WaterWakeMotion(float speed)
{
    return std::clamp(speed / kFullWakeSpeed, 0.0f, 1.0f);
}

float WaterFootprintDepthShare(float motion)
{
    return kStandingFootprintShare + (1.0f - kStandingFootprintShare) * motion;
}

float WaterFootprintFade(double sincePlaced, double sinceSeen)
{
    return static_cast<float>(SmoothUnitStep(sincePlaced / kWaterFootprintFadeSeconds) *
                              SmoothUnitStep(1.0 - sinceSeen / kStaleWaterTrackSeconds));
}

bool PlausibleWaterWakeStep(float distance, double seconds)
{
    return distance <= kMaxWaterWakeSpeed * std::max(seconds, 0.0) + kWaterWakeStepSlack;
}

float WaterSplashRadius(float collisionRadius)
{
    return std::clamp(kSplashRadiusPerCollisionRadius * collisionRadius, kMinSplashRadius, kMaxSplashRadius);
}

float WaterEntryImpulse(float depth, float height)
{
    return kEntryImpulsePerStrength * ClientRippleStrength(depth, height);
}

void WaterContactTracker::Reset()
{
    m_tracks.clear();
    m_departed.clear();
    m_seconds = -1.0;
    m_contacts = 0;
}

const WaterContactTrack* WaterContactTracker::Find(uint64_t guid) const
{
    for (const WaterContactTrack& track : m_tracks)
        if (track.guid == guid)
            return &track;
    return nullptr;
}

WaterContactTrack& WaterContactTracker::TrackOf(uint64_t guid, bool& created)
{
    for (WaterContactTrack& track : m_tracks)
        if (track.guid == guid)
        {
            created = false;
            return track;
        }
    created = true;
    WaterContactTrack track;
    track.guid = guid;
    m_tracks.push_back(track);
    return m_tracks.back();
}

WaterEntryState WaterContactTracker::FirstEntryState(uint64_t guid)
{
    const auto departed = std::find_if(m_departed.begin(), m_departed.end(),
                                       [guid](const DepartedWaterEntry& entry) { return entry.guid == guid; });
    if (departed == m_departed.end())
        return {};
    const WaterEntryState entry = departed->entry;
    m_departed.erase(departed);
    return entry;
}

void WaterContactTracker::Follow(WaterContactTrack& track, const WaterContact& contact, double seconds, bool created)
{
    if (created)
        track.entry = FirstEntryState(contact.guid);
    if (created ||
        !PlausibleWaterWakeStep(GroundDistance(track.position, contact.position), seconds - track.seenAt))
        PlaceFootprint(track, contact.position, seconds);
    else
        MoveFootprint(track);
    std::copy(contact.position, contact.position + 3, track.position);
    track.seenAt = seconds;
    const float depth = WaterContactDepth(contact);
    track.footprint.radius = WaterFootprintRadius(contact.radius);
    track.footprint.level = WaterFootprintLevel(depth, contact.height);
    track.splashRadius = WaterSplashRadius(contact.radius);
    if (TakeEntrySplash(track.entry, contact, seconds))
        track.pendingSplash += WaterEntryImpulse(depth, contact.height);
}

void WaterContactTracker::RememberEntry(const WaterContactTrack& track)
{
    const DepartedWaterEntry memory = {track.guid, track.entry, track.seenAt};
    auto same = std::find_if(m_departed.begin(), m_departed.end(),
                             [&track](const DepartedWaterEntry& departed) { return departed.guid == track.guid; });
    if (same != m_departed.end())
        *same = memory;
    else if (m_departed.size() < kMaxDepartedWaterEntries)
        m_departed.push_back(memory);
    else
        *std::min_element(m_departed.begin(), m_departed.end(),
                          [](const DepartedWaterEntry& a, const DepartedWaterEntry& b) {
                              return a.seenAt < b.seenAt;
                          }) = memory;
}

void WaterContactTracker::DropStaleTracks(double seconds)
{
    const auto stale = [seconds](const WaterContactTrack& track) {
        return seconds - track.seenAt > kStaleWaterTrackSeconds;
    };
    for (const WaterContactTrack& track : m_tracks)
        if (stale(track))
            RememberEntry(track);
    m_tracks.erase(std::remove_if(m_tracks.begin(), m_tracks.end(), stale), m_tracks.end());
    m_departed.erase(std::remove_if(m_departed.begin(), m_departed.end(),
                                    [seconds](const DepartedWaterEntry& departed) {
                                        return seconds - departed.seenAt > kDepartedEntrySeconds;
                                    }),
                     m_departed.end());
}

void WaterContactTracker::Update(const WaterContactFrame& frame, double seconds)
{
    if (m_seconds >= 0.0 && seconds < m_seconds)
        Reset();
    m_contacts = std::min(frame.count, kMaxWaterContacts);
    for (uint32_t i = 0; i < m_contacts; ++i)
    {
        const WaterContact& contact = frame.contacts[i];
        bool created = false;
        WaterContactTrack& track = TrackOf(contact.guid, created);
        if (!created && track.seenAt == seconds)
            continue;
        Follow(track, contact, seconds, created);
    }
    m_seconds = seconds;
    DropStaleTracks(seconds);
}

void WaterContactTracker::PlaceFootprints(double seconds)
{
    for (WaterContactTrack& track : m_tracks)
        PlaceFootprint(track, track.position, seconds);
}

uint32_t WaterContactTracker::TakeFootprints(double stepSeconds, bool fresh, WaterRippleDisturbance* out,
                                             uint32_t capacity)
{
    uint32_t count = 0;
    for (WaterContactTrack& track : m_tracks)
        if ((track.seenAt == m_seconds) == fresh && count < capacity)
            out[count++] = FootprintStep(track, stepSeconds);
    return count;
}

uint32_t WaterContactTracker::TakeSplashes(WaterRippleDisturbance* out, uint32_t capacity)
{
    uint32_t count = 0;
    for (WaterContactTrack& track : m_tracks)
    {
        if (track.pendingSplash == 0.0f || count == capacity)
            continue;
        out[count++] = SplashOf(track);
        track.pendingSplash = 0.0f;
    }
    return count;
}

uint32_t WaterContactTracker::TakeDisturbances(double stepSeconds, WaterRippleDisturbance* out, uint32_t capacity)
{
    uint32_t count = TakeFootprints(stepSeconds, true, out, capacity);
    count += TakeFootprints(stepSeconds, false, out + count, capacity - count);
    return count + TakeSplashes(out + count, capacity - count);
}
