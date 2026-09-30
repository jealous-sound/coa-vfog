#pragma once

#include "engine_actors.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

constexpr size_t kMaxDepartedWaterEntries = 256;
constexpr size_t kMaxPendingWaterRings = 128;
constexpr float kWaterSplashRearmDepthPerHeight = 0.15f;
constexpr double kMinWaterSplashIntervalSeconds = 2.0;

struct WaterRippleDisturbance
{
    float from[2] = {};
    float to[2] = {};
    float radius = 0.0f;
    float amplitude = 0.0f;
};

enum class ClientRippleKind
{
    Idle,
    Turning,
    Moving,
};

struct ClientRipple
{
    ClientRippleKind kind = ClientRippleKind::Idle;
    float size = 0.0f;
    float alpha = 0.0f;
};

struct WaterEntryState
{
    bool armed = false;
    double splashedAt = -std::numeric_limits<double>::infinity();
};

struct WaterContactTrack
{
    uint64_t guid = 0;
    float position[3] = {};
    double seenAt = 0.0;
    WaterEntryState entry;
    float splashRadius = 0.0f;
    float pendingSplash = 0.0f;
    uint32_t nextRippleMs = 0;
    uint32_t rings = 0;
};

struct DepartedWaterEntry
{
    uint64_t guid = 0;
    WaterEntryState entry;
    double seenAt = 0.0;
};

float ClientRippleStrength(float depth, float height);
ClientRippleKind ClientRippleKindOf(uint32_t movementFlags);
float ClientRippleSizeJitter(uint64_t guid, uint32_t nextRippleMs);
ClientRipple ClientRippleOf(ClientRippleKind kind, float scale, float strength, float jitter);
bool ClientRippleEmitted(uint32_t previousMs, uint32_t nextRippleMs);
WaterRippleDisturbance WaterRingImpulse(const float position[2], const ClientRipple& ripple);
float WaterSplashRadius(float collisionRadius);
float WaterEntryImpulse(float depth, float height);

class WaterContactTracker
{
public:
    void Reset();
    void Update(const WaterContactFrame& frame, double seconds);
    uint32_t TakeDisturbances(WaterRippleDisturbance* out, uint32_t capacity);
    bool Emitting() const;
    uint32_t Tracks() const { return static_cast<uint32_t>(m_tracks.size()); }
    uint32_t Contacts() const { return m_contacts; }
    uint32_t RememberedEntries() const { return static_cast<uint32_t>(m_departed.size()); }
    uint32_t PendingRings() const { return static_cast<uint32_t>(m_rings.size()); }
    const WaterContactTrack* Find(uint64_t guid) const;

private:
    WaterContactTrack& TrackOf(uint64_t guid, bool& created);
    WaterEntryState FirstEntryState(uint64_t guid);
    void Follow(WaterContactTrack& track, const WaterContact& contact, double seconds, bool created);
    void QueueRing(WaterContactTrack& track, const WaterContact& contact);
    uint32_t TakeRings(WaterRippleDisturbance* out, uint32_t capacity);
    void RememberEntry(const WaterContactTrack& track);
    void DropStaleTracks(double seconds);

    std::vector<WaterContactTrack> m_tracks;
    std::vector<DepartedWaterEntry> m_departed;
    std::vector<WaterRippleDisturbance> m_rings;
    double m_seconds = -1.0;
    uint32_t m_contacts = 0;
};
