#pragma once

namespace water_ripple_checks
{
using water_contact_checks::ContactAt;
using water_contact_checks::DisturbancesNow;
using water_contact_checks::FrameOf;
using water_contact_checks::kRunSpeed;
using water_contact_checks::kStartSeconds;
using water_contact_checks::kWadingDepth;

constexpr int kTrackerFrameRates[] = {30, 60, 144};
constexpr int kReferenceFrameRates[] = {144, 37};
constexpr double kTrackedSeconds = 2.0;
constexpr float kTrackTolerance = 1e-3f;
constexpr int kTexels = kWaterRippleTexelsLow;
constexpr int kLongRunSteps = 3000;
constexpr int kLongRunImpulsePeriod = 10;
constexpr float kLongRunBound = 8.0f;
constexpr int kSpreadSteps = 30;
constexpr int kReferenceSteps = 90;
constexpr float kImpulseAmplitude = -0.5f;
constexpr float kImpulseRadius = 0.75f;
constexpr int kPlaneWavelengthTexels = 32;
constexpr int kPlaneTexels = kWaterRippleTexels;
constexpr int kPlaneChecks[] = {15, 30, 60};
constexpr float kPlaneAmplitude = 0.5f;
constexpr float kPlaneTolerance = 0.002f;
constexpr int kEdgeRampTexels = kPlaneTexels / 16;
constexpr float kReferenceTolerance = 2e-3f;
constexpr int kRecentreSteps = 40;
constexpr int kRecentreShiftPerStep = 3;
constexpr int kTimedSteps = 32;
constexpr int kTimedDisturbances[] = {0, 4, 32};

struct StepRecord
{
    double seconds;
    WaterRippleDisturbance disturbance;
};

std::vector<StepRecord> TrackAtFrameRate(int framesPerSecond)
{
    WaterContactTracker tracker;
    WaterRipples clock;
    std::vector<StepRecord> steps;
    const double end = kStartSeconds + kTrackedSeconds;
    for (int frame = 0;; ++frame)
    {
        const double t = std::min(kStartSeconds + static_cast<double>(frame) / framesPerSecond, end);
        const float x = static_cast<float>(kRunSpeed * (t - kStartSeconds));
        tracker.Update(FrameOf({ContactAt(1, x, 0.0f, kWadingDepth)}), t);
        const WaterRippleSchedule schedule = clock.Schedule(t);
        for (int step = 0; step < schedule.steps; ++step)
        {
            WaterRippleDisturbance d[kMaxWaterRippleDisturbances];
            if (DisturbancesNow(tracker, schedule.stepSeconds[step], d))
                steps.push_back({schedule.stepSeconds[step], d[0]});
        }
        if (t >= end)
            return steps;
    }
}

void CheckTrackerFrameRateIndependence()
{
    const std::vector<StepRecord> reference = TrackAtFrameRate(kTrackerFrameRates[0]);
    bool same = !reference.empty();
    for (int framesPerSecond : kTrackerFrameRates)
    {
        const std::vector<StepRecord> steps = TrackAtFrameRate(framesPerSecond);
        std::printf("     %3d fps: %zu footprint steps\n", framesPerSecond, steps.size());
        same = same && steps.size() == reference.size();
        for (size_t i = 0; same && i < steps.size(); ++i)
        {
            const WaterRippleDisturbance& a = steps[i].disturbance;
            const WaterRippleDisturbance& b = reference[i].disturbance;
            same = steps[i].seconds == reference[i].seconds && std::fabs(a.to[0] - b.to[0]) < kTrackTolerance &&
                   std::fabs(a.from[0] - b.from[0]) < kTrackTolerance &&
                   std::fabs(a.amplitude - b.amplitude) < kTrackTolerance;
        }
    }
    Check(same, "a unit running at 7 yd/s leaves the same footprint at every 30 Hz step at 30, 60 and 144 fps");
}

enum class HalfRounding
{
    Nearest,
    TowardZero,
};

constexpr HalfRounding kHalfRoundings[] = {HalfRounding::Nearest, HalfRounding::TowardZero};

const char* HalfRoundingName(HalfRounding rounding)
{
    return rounding == HalfRounding::Nearest ? "round to nearest" : "truncation toward zero";
}

bool RoundsUp(uint32_t kept, uint32_t remainder, uint32_t midpoint, HalfRounding rounding)
{
    return rounding == HalfRounding::Nearest && (remainder > midpoint || (remainder == midpoint && (kept & 1)));
}

uint16_t FloatToHalf(float value, HalfRounding rounding = HalfRounding::Nearest)
{
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    const uint32_t sign = (bits >> 16) & 0x8000u;
    const int exponent = static_cast<int>((bits >> 23) & 0xFF) - 127 + 15;
    uint32_t mantissa = bits & 0x7FFFFFu;
    if (exponent >= 31)
        return static_cast<uint16_t>(sign | 0x7C00u);
    if (exponent <= 0)
    {
        if (exponent < -10)
            return static_cast<uint16_t>(sign);
        mantissa |= 0x800000u;
        const int shift = 14 - exponent;
        uint32_t half = mantissa >> shift;
        if (RoundsUp(half, mantissa & ((1u << shift) - 1), 1u << (shift - 1), rounding))
            ++half;
        return static_cast<uint16_t>(sign | half);
    }
    uint32_t half = (static_cast<uint32_t>(exponent) << 10) | (mantissa >> 13);
    if (RoundsUp(half, mantissa & 0x1FFFu, 0x1000u, rounding))
        ++half;
    return static_cast<uint16_t>(sign | half);
}

float RoundToHalf(float value, HalfRounding rounding)
{
    return water_fft_checks::HalfToFloat(FloatToHalf(value, rounding));
}

struct RippleState
{
    int texels = 0;
    std::vector<float> r;
    std::vector<float> g;

    float R(int x, int y) const { return At(r, x, y); }
    float G(int x, int y) const { return At(g, x, y); }

private:
    float At(const std::vector<float>& channel, int x, int y) const
    {
        const size_t index = static_cast<size_t>(y) * texels + x;
        const bool inside = x >= 0 && y >= 0 && x < texels && y < texels && index < channel.size();
        return inside ? channel[index] : std::numeric_limits<float>::quiet_NaN();
    }
};

RippleState ZeroState(int texels)
{
    RippleState state;
    state.texels = texels;
    state.r.assign(static_cast<size_t>(texels) * texels, 0.0f);
    state.g = state.r;
    return state;
}

int ChannelsOf(D3DFORMAT format)
{
    return format == D3DFMT_G16R16F ? 2 : 4;
}

bool ReadRipples(IDirect3DDevice9* dev, const WaterRipples& ripples, RippleState& out)
{
    IDirect3DSurface9* surface = nullptr;
    IDirect3DSurface9* copy = nullptr;
    D3DSURFACE_DESC desc = {};
    bool read = ripples.Map() && SUCCEEDED(ripples.Map()->GetSurfaceLevel(0, &surface)) &&
                SUCCEEDED(surface->GetDesc(&desc)) &&
                SUCCEEDED(dev->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM,
                                                           &copy, nullptr)) &&
                SUCCEEDED(dev->GetRenderTargetData(surface, copy));
    D3DLOCKED_RECT locked = {};
    read = read && SUCCEEDED(copy->LockRect(&locked, nullptr, D3DLOCK_READONLY));
    if (read)
    {
        out = ZeroState(static_cast<int>(desc.Width));
        const int channels = ChannelsOf(desc.Format);
        for (UINT y = 0; y < desc.Height; ++y)
        {
            const auto* row = reinterpret_cast<const uint16_t*>(static_cast<const BYTE*>(locked.pBits) +
                                                                static_cast<size_t>(y) * locked.Pitch);
            for (UINT x = 0; x < desc.Width; ++x)
            {
                out.r[static_cast<size_t>(y) * desc.Width + x] = water_fft_checks::HalfToFloat(row[x * channels]);
                out.g[static_cast<size_t>(y) * desc.Width + x] = water_fft_checks::HalfToFloat(row[x * channels + 1]);
            }
        }
        copy->UnlockRect();
    }
    if (copy)
        copy->Release();
    if (surface)
        surface->Release();
    return read;
}

bool UploadRipples(IDirect3DDevice9* dev, const WaterRipples& ripples, const RippleState& state)
{
    IDirect3DTexture9* staging = nullptr;
    const UINT side = static_cast<UINT>(state.texels);
    if (!ripples.Map() || FAILED(dev->CreateTexture(side, side, 1, 0, ripples.Format(), D3DPOOL_SYSTEMMEM, &staging,
                                                    nullptr)))
        return false;
    D3DLOCKED_RECT locked = {};
    bool uploaded = SUCCEEDED(staging->LockRect(0, &locked, nullptr, 0));
    const int channels = ChannelsOf(ripples.Format());
    for (UINT y = 0; uploaded && y < side; ++y)
    {
        BYTE* rowBytes = static_cast<BYTE*>(locked.pBits) + static_cast<size_t>(y) * locked.Pitch;
        auto* row = reinterpret_cast<uint16_t*>(rowBytes);
        for (UINT x = 0; x < side; ++x)
            for (int c = 0; c < channels; ++c)
                row[x * channels + c] = c == 0   ? FloatToHalf(state.R(x, y))
                                        : c == 1 ? FloatToHalf(state.G(x, y))
                                                 : 0;
    }
    uploaded = uploaded && SUCCEEDED(staging->UnlockRect(0)) && SUCCEEDED(dev->UpdateTexture(staging, ripples.Map()));
    staging->Release();
    return uploaded;
}

float ReferenceSample(const std::vector<float>& channel, int texels, int x, int y)
{
    x = std::clamp(x, 0, texels - 1);
    y = std::clamp(y, 0, texels - 1);
    return channel[static_cast<size_t>(y) * texels + x];
}

float ReferenceFootprint(float x, float y, const WaterRippleDisturbance& d)
{
    const float along[2] = {d.to[0] - d.from[0], d.to[1] - d.from[1]};
    const float lengthSquared = along[0] * along[0] + along[1] * along[1];
    const float t = lengthSquared > 0.0f
                        ? std::clamp(((x - d.from[0]) * along[0] + (y - d.from[1]) * along[1]) / lengthSquared, 0.0f,
                                     1.0f)
                        : 0.0f;
    const float dx = x - d.from[0] - along[0] * t;
    const float dy = y - d.from[1] - along[1] * t;
    const float s = std::clamp((std::sqrt(dx * dx + dy * dy) / d.radius - 0.20f) / (0.79f - 0.20f), 0.0f, 1.0f);
    return d.amplitude * (1.0f - s * s * (3.0f - 2.0f * s));
}

RippleState ReferenceStep(const RippleState& in, int shiftX, int shiftY,
                          const std::vector<WaterRippleDisturbance>& texelDisturbances, HalfRounding rounding)
{
    const int n = in.texels;
    RippleState out = ZeroState(n);
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
        {
            const int sx = x + shiftX;
            const int sy = y + shiftY;
            const float u = (sx + 0.5f) / n;
            const float v = (sy + 0.5f) / n;
            const float edge = std::clamp(std::min(std::min(u, 1.0f - u), std::min(v, 1.0f - v)) * 16.0f, 0.0f, 1.0f);
            const float sum = ReferenceSample(in.r, n, sx - 1, sy) + ReferenceSample(in.r, n, sx, sy - 1) +
                              ReferenceSample(in.r, n, sx + 1, sy) + ReferenceSample(in.r, n, sx, sy + 1);
            float next = 0.97f * edge * (0.5f * sum - ReferenceSample(in.g, n, sx, sy));
            for (const WaterRippleDisturbance& d : texelDisturbances)
                next += ReferenceFootprint(x + 0.5f, y + 0.5f, d);
            out.r[static_cast<size_t>(y) * n + x] = RoundToHalf(next, rounding);
            out.g[static_cast<size_t>(y) * n + x] = ReferenceSample(in.r, n, sx, sy);
        }
    return out;
}

WaterRippleDisturbance InTexels(const WaterRippleDisturbance& world, const int origin[2])
{
    WaterRippleDisturbance d = world;
    for (int axis = 0; axis < 2; ++axis)
    {
        d.from[axis] = world.from[axis] / kWaterRippleTexelYards - origin[axis];
        d.to[axis] = world.to[axis] / kWaterRippleTexelYards - origin[axis];
    }
    d.radius = world.radius / kWaterRippleTexelYards;
    return d;
}

float MaxDifference(const RippleState& a, const RippleState& b)
{
    float worst = 0.0f;
    for (size_t i = 0; i < a.r.size() && i < b.r.size(); ++i)
        worst = std::max(worst, std::max(std::fabs(a.r[i] - b.r[i]), std::fabs(a.g[i] - b.g[i])));
    return a.r.size() == b.r.size() ? worst : std::numeric_limits<float>::infinity();
}

float Peak(const RippleState& state)
{
    float peak = 0.0f;
    for (float value : state.r)
        peak = std::max(peak, std::fabs(value));
    return peak;
}

bool AllFinite(const RippleState& state)
{
    return std::all_of(state.r.begin(), state.r.end(), [](float v) { return std::isfinite(v); }) &&
           std::all_of(state.g.begin(), state.g.end(), [](float v) { return std::isfinite(v); });
}

class RippleBench
{
public:
    RippleBench(IDirect3DDevice9* dev, int texels) : m_dev(dev)
    {
        m_prepared = m_ripples.Prepare(dev, texels);
        m_ripples.Schedule(kStartSeconds);
    }

    bool Prepared() const { return m_prepared; }
    WaterRipples& Ripples() { return m_ripples; }

    void Step(const float centre[2], const std::vector<WaterRippleDisturbance>& disturbances)
    {
        m_ripples.Step(m_dev, centre, disturbances.data(), static_cast<uint32_t>(disturbances.size()));
    }

    RippleState Read()
    {
        RippleState state;
        ReadRipples(m_dev, m_ripples, state);
        return state;
    }

private:
    IDirect3DDevice9* m_dev;
    WaterRipples m_ripples;
    bool m_prepared = false;
};

WaterRippleDisturbance Impulse(float x, float y, float amplitude = kImpulseAmplitude, float radius = kImpulseRadius)
{
    WaterRippleDisturbance d;
    d.from[0] = d.to[0] = x;
    d.from[1] = d.to[1] = y;
    d.radius = radius;
    d.amplitude = amplitude;
    return d;
}

void CheckQuietMapStaysZero(IDirect3DDevice9* dev)
{
    RippleBench bench(dev, kTexels);
    const float centre[2] = {};
    for (int i = 0; i < kSpreadSteps * 2; ++i)
        bench.Step(centre, {});
    const RippleState state = bench.Read();
    const bool zero = !state.r.empty() && std::all_of(state.r.begin(), state.r.end(), [](float v) { return v == 0; }) &&
                      std::all_of(state.g.begin(), state.g.end(), [](float v) { return v == 0; });
    Check(bench.Prepared() && zero, "a ripple map without disturbances stays exactly zero over 60 steps");
}

struct RoundingMatch
{
    HalfRounding rounding = HalfRounding::Nearest;
    float error = std::numeric_limits<float>::infinity();
};

RoundingMatch ClosestRounding(const float (&errors)[2])
{
    RoundingMatch match;
    for (int i = 0; i < 2; ++i)
        if (errors[i] < match.error)
            match = {kHalfRoundings[i], errors[i]};
    return match;
}

void CheckSymmetricSpreadAndReference(IDirect3DDevice9* dev)
{
    RippleBench bench(dev, kTexels);
    const float centre[2] = {};
    const int origin[2] = {-kTexels / 2, -kTexels / 2};
    RippleState references[2] = {ZeroState(kTexels), ZeroState(kTexels)};
    const WaterRippleDisturbance impulse = Impulse(0.0f, 0.0f);
    float worst[2] = {};
    float symmetryError = 0.0f;
    float peakAtSpread = 0.0f;
    for (int step = 1; step <= kReferenceSteps; ++step)
    {
        const std::vector<WaterRippleDisturbance> now = step == 1 ? std::vector<WaterRippleDisturbance>{impulse}
                                                                  : std::vector<WaterRippleDisturbance>{};
        bench.Step(centre, now);
        std::vector<WaterRippleDisturbance> texels;
        for (const WaterRippleDisturbance& d : now)
            texels.push_back(InTexels(d, origin));
        for (int i = 0; i < 2; ++i)
            references[i] = ReferenceStep(references[i], 0, 0, texels, kHalfRoundings[i]);
        if (step % kSpreadSteps)
            continue;
        const RippleState gpu = bench.Read();
        for (int i = 0; i < 2; ++i)
            worst[i] = std::max(worst[i], MaxDifference(gpu, references[i]) / std::max(Peak(references[i]), 1e-6f));
        if (step != kSpreadSteps)
            continue;
        peakAtSpread = Peak(gpu);
        for (int y = 0; y < kTexels; ++y)
            for (int x = 0; x < kTexels; ++x)
            {
                const float value = gpu.R(x, y);
                const float mirrors[3] = {gpu.R(kTexels - 1 - x, y), gpu.R(x, kTexels - 1 - y), gpu.R(y, x)};
                for (float mirrored : mirrors)
                    symmetryError = std::max(symmetryError, std::fabs(value - mirrored));
            }
    }
    const RoundingMatch match = ClosestRounding(worst);
    std::printf("     impulse: peak %.4f after %d steps, worst mirror difference %.2e; over %d steps the GPU is %.2e "
                "of the peak from the CPU reference with FP16 round to nearest and %.2e with truncation\n",
                peakAtSpread, kSpreadSteps, symmetryError, kReferenceSteps, worst[0], worst[1]);
    Check(bench.Prepared() && peakAtSpread > 0.0f && symmetryError <= kReferenceTolerance * peakAtSpread,
          "an impulse at the window centre spreads symmetrically under flips and the diagonal mirror");
    Check(bench.Prepared() && match.error <= kReferenceTolerance,
          (std::string("the GPU ripple steps follow the CPU reference of next = 0.97 edge (0.5 (L + D + R + U) - C.g) "
                       "with the injected footprint and FP16 storage (") +
           HalfRoundingName(match.rounding) + " on this GPU)")
              .c_str());
}

double PlaneAmplitude(const std::vector<float>& row, int steps)
{
    const int margin = kEdgeRampTexels + steps + kPlaneWavelengthTexels;
    const int span = (kPlaneTexels - 2 * margin) / kPlaneWavelengthTexels * kPlaneWavelengthTexels;
    double projection = 0.0;
    double norm = 0.0;
    for (int x = margin; x < margin + span; ++x)
    {
        const double basis = std::cos(2.0 * kPi * x / kPlaneWavelengthTexels);
        projection += row[x] * basis;
        norm += basis * basis;
    }
    return projection / norm;
}

std::vector<float> RowOf(const RippleState& state, int y)
{
    std::vector<float> row(kPlaneTexels);
    for (int x = 0; x < kPlaneTexels; ++x)
        row[x] = state.R(x, y);
    return row;
}

struct PlaneReference
{
    std::vector<float> r;
    std::vector<float> g;

    void Step(HalfRounding rounding)
    {
        std::vector<float> next(r.size());
        for (size_t x = 0; x < r.size(); ++x)
        {
            const float left = r[(x + r.size() - 1) % r.size()];
            const float right = r[(x + 1) % r.size()];
            next[x] = RoundToHalf(0.97f * (0.5f * (left + r[x] + right + r[x]) - g[x]), rounding);
        }
        g = r;
        r = next;
    }
};

void CheckPlaneWaveDecay(IDirect3DDevice9* dev)
{
    RippleBench bench(dev, kPlaneTexels);
    const float centre[2] = {};
    bench.Step(centre, {});
    RippleState plane = ZeroState(kPlaneTexels);
    PlaneReference references[2];
    for (int x = 0; x < kPlaneTexels; ++x)
    {
        const float value = RoundToHalf(kPlaneAmplitude * std::cos(2.0f * kPi * x / kPlaneWavelengthTexels),
                                        HalfRounding::Nearest);
        for (int y = 0; y < kPlaneTexels; ++y)
        {
            plane.r[static_cast<size_t>(y) * kPlaneTexels + x] = value;
            plane.g[static_cast<size_t>(y) * kPlaneTexels + x] = value;
        }
        for (PlaneReference& reference : references)
        {
            reference.r.push_back(value);
            reference.g.push_back(value);
        }
    }
    const bool uploaded = bench.Prepared() && UploadRipples(dev, bench.Ripples(), plane);
    const double beta = 1.0 + std::cos(2.0 * kPi / kPlaneWavelengthTexels);
    double current = kPlaneAmplitude;
    double previous = kPlaneAmplitude;
    bool inside = uploaded;
    float errors[2] = {};
    int done = 0;
    for (int check : kPlaneChecks)
    {
        for (; done < check; ++done)
        {
            bench.Step(centre, {});
            for (int i = 0; i < 2; ++i)
                references[i].Step(kHalfRoundings[i]);
            const double next = 0.97 * (beta * current - previous);
            previous = current;
            current = next;
        }
        const double measured = PlaneAmplitude(RowOf(bench.Read(), kPlaneTexels / 2), check);
        const double envelope =
            kPlaneAmplitude * std::pow(0.97, check / 2.0) / std::sqrt(1.0 - beta * beta * 0.97 / 4.0);
        const double nearest = PlaneAmplitude(references[0].r, check);
        const double truncated = PlaneAmplitude(references[1].r, check);
        std::printf("     plane wave of %d texels after %d steps: GPU %.4f, FP16 nearest %.4f, FP16 truncated %.4f, "
                    "exact %.4f, envelope %.4f\n",
                    kPlaneWavelengthTexels, check, measured, nearest, truncated, current, envelope);
        errors[0] = std::max(errors[0], static_cast<float>(std::fabs(measured - nearest)));
        errors[1] = std::max(errors[1], static_cast<float>(std::fabs(measured - truncated)));
        inside = inside && std::fabs(current) <= envelope;
    }
    const RoundingMatch match = ClosestRounding(errors);
    Check(inside && match.error <= kPlaneTolerance * kPlaneAmplitude,
          (std::string("a plane wave decays as the damped recurrence does, inside an envelope shrinking by sqrt(0.97) "
                       "per step (2.2 s amplitude e-folding at 30 Hz), plus the FP16 storage (") +
           HalfRoundingName(match.rounding) + " on this GPU)")
              .c_str());
}

void CheckEdgeAndLongRun(IDirect3DDevice9* dev)
{
    RippleBench bench(dev, kTexels);
    const int origin[2] = {-kTexels / 2, -kTexels / 2};
    const float centre[2] = {};
    const float nearEdge = (kTexels / 2 - 20) * kWaterRippleTexelYards;
    const WaterRippleDisturbance impulse = Impulse(nearEdge, 0.0f);
    RippleState references[2];
    bench.Step(centre, {impulse});
    for (int i = 0; i < 2; ++i)
        references[i] = ReferenceStep(ZeroState(kTexels), 0, 0, {InTexels(impulse, origin)}, kHalfRoundings[i]);
    for (int step = 1; step < kReferenceSteps; ++step)
    {
        bench.Step(centre, {});
        for (int i = 0; i < 2; ++i)
            references[i] = ReferenceStep(references[i], 0, 0, {}, kHalfRoundings[i]);
    }
    const RippleState gpu = bench.Read();
    float border = 0.0f;
    float referenceBorder = 0.0f;
    for (int y = 0; y < kTexels; ++y)
    {
        border = std::max(border, std::fabs(gpu.R(kTexels - 1, y)));
        referenceBorder = std::max(referenceBorder, std::fabs(references[0].R(kTexels - 1, y)));
    }
    float errors[2] = {};
    for (int i = 0; i < 2; ++i)
        errors[i] = MaxDifference(gpu, references[i]) / std::max(Peak(references[i]), 1e-6f);
    const RoundingMatch match = ClosestRounding(errors);
    const float shiftedCentre[2] = {4 * kWaterRippleTexelYards, 0.0f};
    bench.Step(shiftedCentre, {});
    const RippleState shifted = bench.Read();
    bool entered = !shifted.r.empty();
    for (int y = 0; y < kTexels; ++y)
        for (int x = kTexels - 4; x < kTexels; ++x)
            entered = entered && shifted.R(x, y) == 0.0f;
    std::printf("     impulse 20 texels from the edge: outermost column peak %.2e (reference %.2e), reference "
                "difference %.2e of the peak with %s; shifted-in texels zero %d\n",
                border, referenceBorder, match.error, HalfRoundingName(match.rounding), entered);
    Check(bench.Prepared() && match.error <= kReferenceTolerance && entered,
          "near the window edge the ripple follows Forever's 1/16 edge ramp on the shifted uv, and texels shifted in "
          "from outside start with zero height");

    RippleBench longRun(dev, kTexels);
    water_fft_checks::Lcg random(20260930u);
    bool bounded = longRun.Prepared();
    float highest = 0.0f;
    for (int step = 1; step <= kLongRunSteps && bounded; ++step)
    {
        std::vector<WaterRippleDisturbance> now;
        if (step % kLongRunImpulsePeriod == 0)
            now.push_back(Impulse(static_cast<float>(random.Next() * 12.0), static_cast<float>(random.Next() * 12.0),
                                  static_cast<float>(random.Next()), 0.5f + 0.4f * static_cast<float>(random.Next())));
        longRun.Step(centre, now);
        if (step % 500)
            continue;
        const RippleState state = longRun.Read();
        highest = std::max(highest, Peak(state));
        bounded = AllFinite(state) && Peak(state) < kLongRunBound;
    }
    std::printf("     %d FP16 steps with random impulses every %d steps: highest peak %.3f\n", kLongRunSteps,
                kLongRunImpulsePeriod, highest);
    Check(bounded, "3000 FP16 ripple steps with random impulses stay finite and bounded");
}

RippleState WindowRegion(const RippleState& state, const int origin[2], const int world[2], int side)
{
    RippleState region = ZeroState(side);
    for (int y = 0; y < side; ++y)
        for (int x = 0; x < side; ++x)
        {
            const int sx = world[0] + x - origin[0];
            const int sy = world[1] + y - origin[1];
            region.r[static_cast<size_t>(y) * side + x] = state.R(sx, sy);
            region.g[static_cast<size_t>(y) * side + x] = state.G(sx, sy);
        }
    return region;
}

void CheckRecentringKeepsRipplesInPlace(IDirect3DDevice9* dev)
{
    RippleBench fixed(dev, kWaterRippleTexels);
    RippleBench moving(dev, kWaterRippleTexels);
    const float still[2] = {};
    const WaterRippleDisturbance impulse = Impulse(0.0f, 0.0f);
    float followed[2] = {};
    for (int step = 0; step < kRecentreSteps; ++step)
    {
        const std::vector<WaterRippleDisturbance> now = step == 0 ? std::vector<WaterRippleDisturbance>{impulse}
                                                                  : std::vector<WaterRippleDisturbance>{};
        fixed.Step(still, now);
        moving.Step(followed, now);
        followed[0] += kRecentreShiftPerStep * kWaterRippleTexelYards;
    }
    const RippleState a = fixed.Read();
    const RippleState b = moving.Read();
    const int shift = kRecentreShiftPerStep * (kRecentreSteps - 1);
    const int fixedOrigin[2] = {-kWaterRippleTexels / 2, -kWaterRippleTexels / 2};
    const int movingOrigin[2] = {fixedOrigin[0] + shift, fixedOrigin[1]};
    const int side = kRecentreSteps * 2;
    const int world[2] = {-side / 2, -side / 2};
    const float difference = MaxDifference(WindowRegion(a, fixedOrigin, world, side),
                                           WindowRegion(b, movingOrigin, world, side));
    const float peak = Peak(WindowRegion(a, fixedOrigin, world, side));
    std::printf("     window moved %d texels in whole-texel shifts: peak %.4f, largest difference %.2e\n", shift, peak,
                difference);
    Check(fixed.Prepared() && moving.Prepared() && peak > 0.0f && difference == 0.0f,
          "recentring by whole-texel shifts keeps a ripple bit-identical at the same world position");
}

RippleState RunAtFrameRate(IDirect3DDevice9* dev, int framesPerSecond, uint64_t& steps)
{
    WaterContactTracker tracker;
    RippleState state;
    WaterRipples ripples;
    const bool prepared = ripples.Prepare(dev, kTexels);
    const double end = kStartSeconds + kTrackedSeconds + 0.01;
    const float centre[2] = {};
    for (int frame = 0; prepared; ++frame)
    {
        const double t = std::min(kStartSeconds + static_cast<double>(frame) / framesPerSecond, end);
        const float x = static_cast<float>(-6.0 + kRunSpeed * (t - kStartSeconds));
        tracker.Update(FrameOf({ContactAt(1, x, 0.0f, kWadingDepth)}), t);
        const WaterRippleSchedule schedule = ripples.Schedule(t);
        for (int step = 0; step < schedule.steps; ++step)
        {
            WaterRippleDisturbance d[kMaxWaterRippleDisturbances];
            const uint32_t count = DisturbancesNow(tracker, schedule.stepSeconds[step], d);
            ripples.Step(dev, centre, d, count);
        }
        if (t >= end)
            break;
    }
    steps = ripples.StepsRun();
    ReadRipples(dev, ripples, state);
    return state;
}

void CheckSimulationFrameRateIndependence(IDirect3DDevice9* dev)
{
    uint64_t steps[2] = {};
    const RippleState fast = RunAtFrameRate(dev, kReferenceFrameRates[0], steps[0]);
    const RippleState slow = RunAtFrameRate(dev, kReferenceFrameRates[1], steps[1]);
    const float difference = MaxDifference(fast, slow);
    const float peak = Peak(fast);
    std::printf("     running unit for %.2f s at %d and %d fps: %llu and %llu steps, peak %.4f, largest difference "
                "%.2e\n",
                kTrackedSeconds + 0.01, kReferenceFrameRates[0], kReferenceFrameRates[1],
                static_cast<unsigned long long>(steps[0]), static_cast<unsigned long long>(steps[1]), peak,
                difference);
    Check(steps[0] == steps[1] && steps[0] > 0 && peak > 0.0f && difference <= kReferenceTolerance * peak,
          "the same elapsed time runs the same 30 Hz steps and leaves the same ripples at 144 and 37 fps");
}

void CheckQuietSimulationStops(IDirect3DDevice9* dev)
{
    RippleBench bench(dev, kTexels);
    const float centre[2] = {};
    bench.Step(centre, {Impulse(0.0f, 0.0f)});
    for (uint32_t i = 1; i < kWaterRippleQuietSteps; ++i)
        bench.Step(centre, {});
    const bool runningBefore = bench.Ripples().Running();
    bench.Step(centre, {});
    Check(bench.Prepared() && runningBefore && !bench.Ripples().Running() && !bench.Ripples().Visible(),
          "after 15 s (450 steps) without disturbances the ripple simulation stops and its map is no longer sampled");
}

void ReportStepCost(IDirect3DDevice9* dev)
{
    water_fft_checks::WarmUpGpuClocks(dev);
    for (int texels : {kWaterRippleTexelsLow, kWaterRippleTexels})
        for (int disturbances : kTimedDisturbances)
        {
            RippleBench bench(dev, texels);
            std::vector<WaterRippleDisturbance> now;
            for (int i = 0; i < disturbances; ++i)
                now.push_back(Impulse(0.5f * i, 0.25f * i, 0.001f, 0.6f));
            water_fft_checks::WaterStopwatch stopwatches[kTimedSteps];
            bool measured = bench.Prepared();
            const float centre[2] = {};
            for (int i = 0; i < kTimedSteps && measured; ++i)
            {
                measured = stopwatches[i].Create(dev);
                stopwatches[i].Start();
                bench.Step(centre, now);
                stopwatches[i].Stop();
            }
            std::vector<double> times;
            for (int i = 0; i < kTimedSteps && measured; ++i)
            {
                double ms = -1.0;
                DWORD pixels = 0;
                measured = stopwatches[i].Read(ms, pixels);
                if (ms >= 0.0)
                    times.push_back(ms);
            }
            std::printf("     ripple step %d x %d with %d disturbances: median %.4f ms GPU (%zu timed steps)\n",
                        texels, texels, disturbances, water_fft_checks::Median(times), times.size());
        }
}

void CheckWaterRipples(IDirect3DDevice9* dev)
{
    CheckTrackerFrameRateIndependence();

    IDirect3DStateBlock9* state = nullptr;
    IDirect3DSurface9* target = nullptr;
    IDirect3DSurface9* depth = nullptr;
    dev->CreateStateBlock(D3DSBT_ALL, &state);
    dev->GetRenderTarget(0, &target);
    dev->GetDepthStencilSurface(&depth);
    WaterRipples probe;
    const bool supported = probe.Prepare(dev, kTexels);
    std::printf("     water ripples on this device: %s (%s)\n", supported ? "supported" : probe.LastFailure(),
                probe.Format() == D3DFMT_G16R16F ? "G16R16F" : "A16B16G16R16F");
    Check(supported, "the ripple simulation's targets and step shader are created on the harness device");
    probe.ReleaseAll();
    if (supported && SUCCEEDED(dev->BeginScene()))
    {
        CheckQuietMapStaysZero(dev);
        CheckSymmetricSpreadAndReference(dev);
        CheckPlaneWaveDecay(dev);
        CheckEdgeAndLongRun(dev);
        CheckRecentringKeepsRipplesInPlace(dev);
        CheckSimulationFrameRateIndependence(dev);
        CheckQuietSimulationStops(dev);
        ReportStepCost(dev);
        dev->EndScene();
    }
    if (state)
    {
        state->Apply();
        state->Release();
    }
    dev->SetRenderTarget(0, target);
    if (target)
        target->Release();
    dev->SetDepthStencilSurface(depth);
    if (depth)
        depth->Release();
}
}
