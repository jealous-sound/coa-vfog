#pragma once

#include "engine_actors.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

constexpr size_t kMaxDepartedWaterEntries = 256;
constexpr float kWaterSplashRearmDepthPerHeight = 0.15f;
constexpr double kMinWaterSplashIntervalSeconds = 2.0;
constexpr double kStaleWaterTrackSeconds = 0.5;
constexpr double kWaterFootprintFadeSeconds = 1.0;
constexpr double kWaterWakeRampSeconds = 0.3;
constexpr float kMaxWaterWakeSpeed = 40.0f;
constexpr float kWaterWakeStepSlack = 0.5f;

struct WaterRippleDisturbance
{
    float from[2] = {};
    float to[2] = {};
    float radius = 0.0f;
    float amplitude = 0.0f;
    bool held = false;
};

struct WaterEntryState
{
    bool armed = false;
    double splashedAt = -std::numeric_limits<double>::infinity();
};

struct WaterFootprint
{
    float from[2] = {};
    double fromSeconds = 0.0;
    float stamped[2] = {};
    double placedAt = 0.0;
    double steppedAt = -std::numeric_limits<double>::infinity();
    float radius = 0.0f;
    float level = 0.0f;
    float motion = 0.0f;
};

struct WaterContactTrack
{
    uint64_t guid = 0;
    float position[3] = {};
    double seenAt = 0.0;
    WaterEntryState entry;
    WaterFootprint footprint;
    uint32_t placements = 0;
    float splashRadius = 0.0f;
    float pendingSplash = 0.0f;
};

struct DepartedWaterEntry
{
    uint64_t guid = 0;
    WaterEntryState entry;
    double seenAt = 0.0;
};

float ClientRippleStrength(float depth, float height);
float WaterFootprintRadius(float collisionRadius);
float WaterFootprintLevel(float depth, float height);
float WaterWakeMotion(float speed);
float WaterFootprintDepthShare(float motion);
float WaterFootprintFade(double sincePlaced, double sinceSeen);
bool PlausibleWaterWakeStep(float distance, double seconds);
float WaterSplashRadius(float collisionRadius);
float WaterEntryImpulse(float depth, float height);

class WaterContactTracker
{
public:
    void Reset();
    void Update(const WaterContactFrame& frame, double seconds);
    void PlaceFootprints(double seconds);
    uint32_t TakeDisturbances(double stepSeconds, WaterRippleDisturbance* out, uint32_t capacity);
    bool Emitting() const { return !m_tracks.empty(); }
    uint32_t Tracks() const { return static_cast<uint32_t>(m_tracks.size()); }
    uint32_t Contacts() const { return m_contacts; }
    uint32_t RememberedEntries() const { return static_cast<uint32_t>(m_departed.size()); }
    const WaterContactTrack* Find(uint64_t guid) const;

private:
    WaterContactTrack& TrackOf(uint64_t guid, bool& created);
    WaterEntryState FirstEntryState(uint64_t guid);
    void Follow(WaterContactTrack& track, const WaterContact& contact, double seconds, bool created);
    uint32_t TakeFootprints(double stepSeconds, bool fresh, WaterRippleDisturbance* out, uint32_t capacity);
    uint32_t TakeSplashes(WaterRippleDisturbance* out, uint32_t capacity);
    void RememberEntry(const WaterContactTrack& track);
    void DropStaleTracks(double seconds);

    std::vector<WaterContactTrack> m_tracks;
    std::vector<DepartedWaterEntry> m_departed;
    double m_seconds = -1.0;
    uint32_t m_contacts = 0;
};
