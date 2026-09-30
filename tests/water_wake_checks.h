#pragma once

namespace water_wake_checks
{
using water_checks::BasinClient;
using water_checks::BasinContact;
using water_checks::ContactsOf;
using water_checks::RenderRippleFrame;
using water_checks::WaterView;

constexpr double kWakeStart = 300.0;
constexpr double kWakeFrame = 1.0 / 30.0;
constexpr float kWakeUnitX = 65.0f;
constexpr float kWakeShinDepth = 0.5f;
constexpr double kIdleSettleSeconds = 3.0;
constexpr double kIdleWatchSeconds = 1.0;
constexpr float kCalmVelocity = 1e-3f;
constexpr float kDefaultRipples = 0.5f;
constexpr float kRunSpeed = 7.0f;
constexpr float kWakeStartY = -5.0f;
constexpr double kStandBeforeSeconds = 2.5;
constexpr double kRunSeconds = 1.5;
constexpr double kStandAfterSeconds = 3.5;
constexpr double kStandSettleSeconds = 2.0;
constexpr double kPeakWindowSeconds = 0.2;
constexpr double kQuietShare = 1e-4;
constexpr double kSettledShare = 0.1;
constexpr double kEnergyWobble = 0.02;

struct MapMotion
{
    float velocity = 0.0f;
    double energy = 0.0;
};

MapMotion MotionOf(const water_ripple_checks::RippleState& map)
{
    MapMotion motion;
    for (size_t i = 0; i < map.r.size(); ++i)
    {
        const float v = map.r[i] - map.g[i];
        motion.velocity = std::max(motion.velocity, std::fabs(v));
        motion.energy += static_cast<double>(v) * v;
    }
    return motion;
}

bool ReadShadedRipples(Harness& h, water_ripple_checks::RippleState& map)
{
    WaterRippleShading shading;
    vf_test_water_ripple_shading(&shading);
    return shading.map && water_ripple_checks::ReadRippleMap(h.dev, shading.map, map);
}

WaterContact StandingWader()
{
    return BasinContact(1, kWakeUnitX, 0.0f, kWakeShinDepth);
}

void CheckStandingUnitMakesNoWaves(Harness& h, BasinClient& client, const Config& base)
{
    const WaterView view = client.View();
    water_checks::ReleaseRipples(client, base, kWakeStart);
    const Config on = water_checks::RippleConfig(base, kDefaultRipples);
    vf_test_set_config(&on);
    MapMotion worst;
    bool read = true;
    const int frames = static_cast<int>((kIdleSettleSeconds + kIdleWatchSeconds) / kWakeFrame);
    const int watched = static_cast<int>(kIdleSettleSeconds / kWakeFrame);
    for (int frame = 1; frame <= frames; ++frame)
    {
        const double seconds = kWakeStart + frame * kWakeFrame;
        RenderRippleFrame(client, view, seconds, ContactsOf({StandingWader()}));
        if (frame <= watched)
            continue;
        water_ripple_checks::RippleState map;
        read = ReadShadedRipples(h, map) && read;
        const MapMotion motion = MotionOf(map);
        worst.velocity = std::max(worst.velocity, motion.velocity);
        worst.energy = std::max(worst.energy, motion.energy);
    }
    std::printf("     unit standing shin-deep for %.0f s: largest |R - G| %.2e, largest sum (R - G)^2 %.2e over the "
                "last %.0f s\n",
                kIdleSettleSeconds + kIdleWatchSeconds, worst.velocity, worst.energy, kIdleWatchSeconds);
    Check(read && worst.velocity <= kCalmVelocity,
          "a unit standing in the water leaves the ripple map still once its footprint has settled: no texel "
          "changes by more than 0.001 in a step between 3 and 4 s after it appeared");
    water_checks::ReleaseRipples(client, base, kWakeStart + frames * kWakeFrame + water_checks::kRippleGap);
}

WaterContact WaderAt(double sinceStart)
{
    const double running = std::clamp(sinceStart - kStandBeforeSeconds, 0.0, kRunSeconds);
    return BasinContact(1, kWakeUnitX, static_cast<float>(kWakeStartY + kRunSpeed * running), kWakeShinDepth);
}

struct EnergyTrace
{
    std::vector<double> seconds;
    std::vector<double> energy;
    bool read = true;

    double Peak() const { return energy.empty() ? 0.0 : *std::max_element(energy.begin(), energy.end()); }

    double LargestBetween(double from, double to) const
    {
        double largest = 0.0;
        for (size_t i = 0; i < energy.size(); ++i)
            largest = seconds[i] >= from && seconds[i] <= to ? std::max(largest, energy[i]) : largest;
        return largest;
    }

    int RisesBetween(double from, double to) const
    {
        int rises = 0;
        for (size_t i = 1; i < energy.size(); ++i)
            rises += seconds[i - 1] >= from && seconds[i] <= to && energy[i] > energy[i - 1] * (1.0 + kEnergyWobble)
                         ? 1
                         : 0;
        return rises;
    }
};

void CheckStartAndStopEachReleaseOneTransient(Harness& h, BasinClient& client, const Config& base)
{
    const WaterView view = client.View();
    water_checks::ReleaseRipples(client, base, kWakeStart);
    const Config on = water_checks::RippleConfig(base, kDefaultRipples);
    vf_test_set_config(&on);
    EnergyTrace trace;
    const double total = kStandBeforeSeconds + kRunSeconds + kStandAfterSeconds;
    const int frames = static_cast<int>(total / kWakeFrame);
    for (int frame = 1; frame <= frames; ++frame)
    {
        const double since = frame * kWakeFrame;
        RenderRippleFrame(client, view, kWakeStart + since, ContactsOf({WaderAt(since)}));
        if (since < kStandSettleSeconds)
            continue;
        water_ripple_checks::RippleState map;
        trace.read = ReadShadedRipples(h, map) && trace.read;
        trace.seconds.push_back(since);
        trace.energy.push_back(MotionOf(map).energy);
    }
    const double stop = kStandBeforeSeconds + kRunSeconds;
    const double peak = trace.Peak();
    const double standing = trace.LargestBetween(kStandSettleSeconds, kStandBeforeSeconds);
    const double atStop = trace.LargestBetween(stop - kPeakWindowSeconds, stop + kPeakWindowSeconds);
    const double settled = trace.LargestBetween(total - kWakeFrame * 4.0, total);
    const int risesWhileRunning = trace.RisesBetween(kStandBeforeSeconds + kWakeFrame * 4.0, stop);
    const int risesAfterStop = trace.RisesBetween(stop + kWakeFrame * 4.0, total);
    std::printf("     stand %.1f s, run %.1f s, stand %.1f s: map sum (R - G)^2 %.1e of its peak %.3e before the "
                "start, %.2f within 0.2 s of the stop, %.3f at the end; %d rises while running, %d after the stop\n",
                kStandBeforeSeconds, kRunSeconds, kStandAfterSeconds, standing / peak, peak, atStop / peak,
                settled / peak, risesWhileRunning, risesAfterStop);
    Check(trace.read && peak > 0.0 && standing <= kQuietShare * peak && atStop == peak && risesWhileRunning > 0 &&
              risesAfterStop == 0 && settled <= kSettledShare * peak,
          "a unit that stands, runs and stops stirs the water once: the ripple map's motion stays under 0.01% of its "
          "peak while the unit stands, grows while it runs, peaks within 0.2 s of its stop and then only dies away "
          "(below 10% 3.5 s later), never rising again while it stands");
    water_checks::ReleaseRipples(client, base, kWakeStart + total + water_checks::kRippleGap);
}

void CheckWakes(Harness& h)
{
    Config base;
    vf_test_get_config(&base);
    BasinClient client(h, water_checks::MakeWaterView(water_checks::kRippleEye, water_checks::kRippleEyeTarget));
    Check(water_checks::AssignWaterData(water_checks::MakeSyntheticWaterData()),
          "synthetic water data assigned for the wake checks");
    CheckStandingUnitMakesNoWaves(h, client, base);
    CheckStartAndStopEachReleaseOneTransient(h, client, base);
    vf_test_set_water_seconds(water_checks::kRealTime);
    vf_test_set_config(&base);
}
}
