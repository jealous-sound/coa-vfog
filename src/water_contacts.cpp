#include "water_contacts.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kClientSplashDepthPerHeight = 0.4f;
constexpr float kClientFullStrength = 1.0f;
constexpr float kClientDeepestStrength = 0.5f;
constexpr uint32_t kClientMoveFlags = 0xF;
constexpr uint32_t kClientTurnFlags = 0x30;
constexpr float kClientRippleSizePerScale = 1.0f / 3.0f;
constexpr float kMinClientRippleSize = 1.0f / 3.0f;
constexpr float kMaxClientRippleSize = 5.0f / 3.0f;
constexpr float kIdleClientRippleSize = 0.6f;
constexpr float kIdleClientRippleAlpha = 0.8f;
constexpr float kMinClientRippleJitter = 0.9f;
constexpr float kClientRippleJitterRange = 0.2f;
constexpr float kRingStampRadiusPerClientSize = 1.0f;
constexpr float kMinRingStampRadius = 0.25f;
constexpr float kRingImpulsePerClientAlpha = -3.0f;
constexpr float kSplashRadiusPerCollisionRadius = 2.0f;
constexpr float kMinSplashRadius = 0.5f;
constexpr float kMaxSplashRadius = 6.0f;
constexpr double kStaleTrackSeconds = 0.5;
constexpr double kDepartedEntrySeconds = 30.0;
constexpr uint64_t kJitterHashIncrement = 0x9E3779B97F4A7C15ull;
constexpr uint64_t kJitterHashMultiplier1 = 0xBF58476D1CE4E5B9ull;
constexpr uint64_t kJitterHashMultiplier2 = 0x94D049BB133111EBull;
constexpr int kJitterHashBits = 53;
constexpr double kJitterHashScale = 1.0 / static_cast<double>(1ull << kJitterHashBits);

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

uint64_t MixBits(uint64_t x)
{
    x += kJitterHashIncrement;
    x = (x ^ (x >> 30)) * kJitterHashMultiplier1;
    x = (x ^ (x >> 27)) * kJitterHashMultiplier2;
    return x ^ (x >> 31);
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

ClientRippleKind ClientRippleKindOf(uint32_t movementFlags)
{
    if (movementFlags & kClientMoveFlags)
        return ClientRippleKind::Moving;
    if (movementFlags & kClientTurnFlags)
        return ClientRippleKind::Turning;
    return ClientRippleKind::Idle;
}

float ClientRippleSizeJitter(uint64_t guid, uint32_t nextRippleMs)
{
    const uint64_t bits = MixBits(guid ^ MixBits(nextRippleMs)) >> (64 - kJitterHashBits);
    return kMinClientRippleJitter + kClientRippleJitterRange * static_cast<float>(bits * kJitterHashScale);
}

ClientRipple ClientRippleOf(ClientRippleKind kind, float scale, float strength, float jitter)
{
    const float size =
        std::clamp(scale * kClientRippleSizePerScale * jitter, kMinClientRippleSize, kMaxClientRippleSize) * strength;
    const bool idle = kind == ClientRippleKind::Idle;
    return {kind, idle ? kIdleClientRippleSize * size : size, idle ? kIdleClientRippleAlpha * strength : strength};
}

bool ClientRippleEmitted(uint32_t previousMs, uint32_t nextRippleMs)
{
    return nextRippleMs != 0 && nextRippleMs != previousMs;
}

WaterRippleDisturbance WaterRingImpulse(const float position[2], const ClientRipple& ripple)
{
    WaterRippleDisturbance ring;
    for (int axis = 0; axis < 2; ++axis)
        ring.from[axis] = ring.to[axis] = position[axis];
    ring.radius = std::max(kRingStampRadiusPerClientSize * ripple.size, kMinRingStampRadius);
    ring.amplitude = kRingImpulsePerClientAlpha * ripple.alpha;
    return ring;
}

float WaterSplashRadius(float collisionRadius)
{
    return std::clamp(kSplashRadiusPerCollisionRadius * collisionRadius, kMinSplashRadius, kMaxSplashRadius);
}

float WaterEntryImpulse(float depth, float height)
{
    return kRingImpulsePerClientAlpha * ClientRippleStrength(depth, height);
}

void WaterContactTracker::Reset()
{
    m_tracks.clear();
    m_departed.clear();
    m_rings.clear();
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

void WaterContactTracker::QueueRing(WaterContactTrack& track, const WaterContact& contact)
{
    const float strength = ClientRippleStrength(WaterContactDepth(contact), contact.height);
    const ClientRipple ripple = ClientRippleOf(ClientRippleKindOf(contact.movementFlags), contact.scale, strength,
                                               ClientRippleSizeJitter(contact.guid, contact.nextRippleMs));
    if (m_rings.size() == kMaxPendingWaterRings)
        m_rings.erase(m_rings.begin());
    m_rings.push_back(WaterRingImpulse(contact.position, ripple));
    ++track.rings;
}

void WaterContactTracker::Follow(WaterContactTrack& track, const WaterContact& contact, double seconds, bool created)
{
    const bool emitted = !created && ClientRippleEmitted(track.nextRippleMs, contact.nextRippleMs);
    if (created)
        track.entry = FirstEntryState(contact.guid);
    std::copy(contact.position, contact.position + 3, track.position);
    track.seenAt = seconds;
    track.nextRippleMs = contact.nextRippleMs;
    track.splashRadius = WaterSplashRadius(contact.radius);
    if (TakeEntrySplash(track.entry, contact, seconds))
        track.pendingSplash += WaterEntryImpulse(WaterContactDepth(contact), contact.height);
    else if (emitted)
        QueueRing(track, contact);
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
        return seconds - track.seenAt > kStaleTrackSeconds;
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

uint32_t WaterContactTracker::TakeRings(WaterRippleDisturbance* out, uint32_t capacity)
{
    const uint32_t taken = std::min(capacity, static_cast<uint32_t>(m_rings.size()));
    std::copy(m_rings.begin(), m_rings.begin() + taken, out);
    m_rings.erase(m_rings.begin(), m_rings.begin() + taken);
    return taken;
}

uint32_t WaterContactTracker::TakeDisturbances(WaterRippleDisturbance* out, uint32_t capacity)
{
    uint32_t count = 0;
    for (WaterContactTrack& track : m_tracks)
    {
        if (track.pendingSplash == 0.0f)
            continue;
        if (count == capacity)
            return count;
        out[count++] = SplashOf(track);
        track.pendingSplash = 0.0f;
    }
    return count + TakeRings(out + count, capacity - count);
}

bool WaterContactTracker::Emitting() const
{
    if (!m_rings.empty())
        return true;
    for (const WaterContactTrack& track : m_tracks)
        if (track.pendingSplash != 0.0f)
            return true;
    return false;
}
