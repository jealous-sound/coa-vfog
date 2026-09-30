#pragma once

#include "engine_actors.h"

#include <cstdint>
#include <vector>

struct WaterRippleDisturbance
{
    float from[2] = {};
    float to[2] = {};
    float radius = 0.0f;
    float amplitude = 0.0f;
};

struct WaterContactTrack
{
    uint64_t guid = 0;
    float position[3] = {};
    float velocity[2] = {};
    double seenAt = 0.0;
    float previousDepth = 0.0f;
    float radius = 0.0f;
    float footprint = 0.0f;
    float pendingImpulse = 0.0f;
};

float ClientRippleStrength(float depth, float height);
float WaterFootprintRadius(float collisionRadius);
float WaterFootprintAmplitude(float depth, float height, float speed);
float WaterEntryImpulse(float depth, float height);

class WaterContactTracker
{
public:
    void Reset();
    void Update(const WaterContactFrame& frame, double seconds);
    uint32_t DisturbancesAt(double stepSeconds, double stepLength, WaterRippleDisturbance* out, uint32_t capacity);
    bool Emitting() const;
    uint32_t Tracks() const { return static_cast<uint32_t>(m_tracks.size()); }
    uint32_t Contacts() const { return m_contacts; }
    const WaterContactTrack* Find(uint64_t guid) const;

private:
    WaterContactTrack& TrackOf(uint64_t guid, bool& created);
    void Follow(WaterContactTrack& track, const WaterContact& contact, double seconds, bool created, bool seeding);
    void DropStaleTracks(double seconds);
    bool SeenNow(const WaterContactTrack& track) const { return track.seenAt == m_seconds; }

    std::vector<WaterContactTrack> m_tracks;
    double m_seconds = -1.0;
    uint32_t m_contacts = 0;
};
