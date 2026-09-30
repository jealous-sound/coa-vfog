#include "water_contacts.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kClientSplashDepthPerHeight = 0.4f;
constexpr float kClientFullStrength = 1.0f;
constexpr float kClientDeepestStrength = 0.5f;
constexpr float kFootprintRadiusPerCollisionRadius = 2.0f;
constexpr float kMinFootprintRadius = 0.5f;
constexpr float kMaxFootprintRadius = 6.0f;
constexpr float kFootprintDisplacement = -0.04f;
constexpr float kFullFootprintSpeed = 7.0f;
constexpr float kFullImmersionPerHeight = 0.5f;
constexpr float kEntryDisplacement = -0.5f;
constexpr float kTeleportSlackYards = 2.0f;
constexpr float kTeleportSpeedFactor = 3.0f;
constexpr double kMaxTrackGapSeconds = 0.5;
constexpr double kStaleTrackSeconds = 0.5;
constexpr double kDepartedDepthSeconds = 30.0;

float Saturate(float x)
{
    return std::clamp(x, 0.0f, 1.0f);
}

float GroundLength(float x, float y)
{
    return std::sqrt(x * x + y * y);
}

bool Teleported(const float moved[2], float speed, double seconds)
{
    const double allowed = kTeleportSlackYards + kTeleportSpeedFactor * static_cast<double>(speed) * seconds;
    return GroundLength(moved[0], moved[1]) > allowed;
}

bool CrossesSplashDepth(float depth, float previousDepth, float height)
{
    const float splashDepth = kClientSplashDepthPerHeight * height;
    return (depth > splashDepth) != (previousDepth > splashDepth);
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

float WaterFootprintAmplitude(float depth, float height, float speed)
{
    const float immersion = Saturate(depth / (kFullImmersionPerHeight * height));
    return kFootprintDisplacement * immersion * ClientRippleStrength(depth, height) *
           Saturate(speed / kFullFootprintSpeed);
}

float WaterEntryImpulse(float depth, float height)
{
    return kEntryDisplacement * ClientRippleStrength(depth, height);
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

float WaterContactTracker::FirstPreviousDepth(const WaterContact& contact, bool seeding)
{
    const float depth = WaterContactDepth(contact);
    if (seeding)
        return depth;
    for (auto departed = m_departed.begin(); departed != m_departed.end(); ++departed)
        if (departed->guid == contact.guid)
        {
            const float previousDepth = departed->previousDepth;
            m_departed.erase(departed);
            return previousDepth;
        }
    return contact.swimming ? depth : 0.0f;
}

void WaterContactTracker::Follow(WaterContactTrack& track, const WaterContact& contact, double seconds, bool created,
                                 bool seeding)
{
    const float depth = WaterContactDepth(contact);
    if (created)
        track.previousDepth = FirstPreviousDepth(contact, seeding);
    else
    {
        const double elapsed = seconds - track.seenAt;
        const float moved[2] = {contact.position[0] - track.position[0], contact.position[1] - track.position[1]};
        const bool restarted = elapsed > kMaxTrackGapSeconds || Teleported(moved, contact.speed, elapsed);
        for (int axis = 0; axis < 2; ++axis)
            if (restarted)
                track.velocity[axis] = 0.0f;
            else if (elapsed > 0.0)
                track.velocity[axis] = static_cast<float>(moved[axis] / elapsed);
    }
    std::copy(contact.position, contact.position + 3, track.position);
    track.seenAt = seconds;
    track.radius = WaterFootprintRadius(contact.radius);
    track.footprint =
        WaterFootprintAmplitude(depth, contact.height, GroundLength(track.velocity[0], track.velocity[1]));
    if (contact.swimming)
        return;
    if (!seeding && CrossesSplashDepth(depth, track.previousDepth, contact.height))
        track.pendingImpulse += WaterEntryImpulse(depth, contact.height);
    track.previousDepth = depth;
}

void WaterContactTracker::RememberDepth(const WaterContactTrack& track)
{
    const WaterContactDepthMemory memory = {track.guid, track.previousDepth, track.seenAt};
    auto same = std::find_if(m_departed.begin(), m_departed.end(),
                             [&track](const WaterContactDepthMemory& departed) { return departed.guid == track.guid; });
    if (same != m_departed.end())
        *same = memory;
    else if (m_departed.size() < kMaxDepartedWaterContactDepths)
        m_departed.push_back(memory);
    else
        *std::min_element(m_departed.begin(), m_departed.end(),
                          [](const WaterContactDepthMemory& a, const WaterContactDepthMemory& b) {
                              return a.seenAt < b.seenAt;
                          }) = memory;
}

void WaterContactTracker::DropStaleTracks(double seconds)
{
    const auto stale = [seconds](const WaterContactTrack& track) {
        return seconds - track.seenAt > kStaleTrackSeconds;
    };
    for (const WaterContactTrack& track : m_tracks)
        if (stale(track))
            RememberDepth(track);
    m_tracks.erase(std::remove_if(m_tracks.begin(), m_tracks.end(), stale), m_tracks.end());
    m_departed.erase(std::remove_if(m_departed.begin(), m_departed.end(),
                                    [seconds](const WaterContactDepthMemory& departed) {
                                        return seconds - departed.seenAt > kDepartedDepthSeconds;
                                    }),
                     m_departed.end());
}

void WaterContactTracker::Update(const WaterContactFrame& frame, double seconds)
{
    if (m_seconds >= 0.0 && seconds < m_seconds)
        Reset();
    const bool seeding = m_seconds < 0.0;
    m_contacts = std::min(frame.count, kMaxWaterContacts);
    for (uint32_t i = 0; i < m_contacts; ++i)
    {
        const WaterContact& contact = frame.contacts[i];
        bool created = false;
        WaterContactTrack& track = TrackOf(contact.guid, created);
        if (!created && track.seenAt == seconds)
            continue;
        Follow(track, contact, seconds, created, seeding);
    }
    m_seconds = seconds;
    DropStaleTracks(seconds);
}

uint32_t WaterContactTracker::DisturbancesAt(double stepSeconds, double stepLength, WaterRippleDisturbance* out,
                                            uint32_t capacity)
{
    uint32_t count = 0;
    for (WaterContactTrack& track : m_tracks)
    {
        const bool moving = SeenNow(track);
        const float amplitude = (moving ? track.footprint : 0.0f) + track.pendingImpulse;
        if (amplitude == 0.0f)
            continue;
        if (count == capacity)
            break;
        const double sinceSeen = moving ? std::min(stepSeconds - track.seenAt, 0.0) : 0.0;
        WaterRippleDisturbance& d = out[count++];
        for (int axis = 0; axis < 2; ++axis)
        {
            const double velocity = moving ? track.velocity[axis] : 0.0;
            d.to[axis] = static_cast<float>(track.position[axis] + velocity * sinceSeen);
            d.from[axis] = static_cast<float>(track.position[axis] + velocity * (sinceSeen - stepLength));
        }
        d.radius = track.radius;
        d.amplitude = amplitude;
        track.pendingImpulse = 0.0f;
    }
    return count;
}

bool WaterContactTracker::Emitting() const
{
    for (const WaterContactTrack& track : m_tracks)
        if (track.pendingImpulse != 0.0f || (SeenNow(track) && track.footprint != 0.0f))
            return true;
    return false;
}
