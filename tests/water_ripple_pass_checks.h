#pragma once

namespace water_checks
{
constexpr float kRippleUnitX = 65.0f;
constexpr Vec3 kRippleEye = {50, 0, 10};
constexpr Vec3 kRippleEyeTarget = {80, 0, 0};
constexpr float kRippleUnitStartY = -4.0f;
constexpr float kRippleUnitSpeed = 7.0f;
constexpr float kRippleUnitHeight = 2.0f;
constexpr float kRippleUnitRadius = 0.35f;
constexpr float kRippleWadingDepth = 0.6f;
constexpr float kRippleDeepDepth = 1.2f;
constexpr double kRippleStart = 40.0;
constexpr double kRippleFrame = 1.0 / 60.0;
constexpr int kRippleFrames = 60;
constexpr float kRippleReach = 5.0f;
constexpr int kRippleDebugView = 6;
constexpr size_t kMinRipplePixels = 100;
constexpr int kOtherMap = 571;
constexpr double kRippleGap = 1.5;
constexpr int kRippleSummaryFrames = 60;
constexpr BYTE kFlatRippleGrey = 127;
constexpr unsigned kRippleMapsHeld = 0x8;
constexpr int kRippleGreyTolerance = 2;
constexpr float kSlopeCheckGain = 2.0f;
constexpr double kSlopeCheckStepFraction = 0.3;
constexpr double kForeverRippleSlopeScale = 3.0;
constexpr double kRippleFullDetailTexels = 2.0;
constexpr double kRippleDetailFadeTexels = 2.0;
constexpr double kMinRayRise = 1e-4;
constexpr float kDepthCopyPrecision = 1e-3f;
constexpr double kMaxNormalLevels = 1.0;
constexpr double kMaxOutlierNormalLevels = 6.0;
constexpr double kMaxOutlierShare = 5e-3;
constexpr double kMinTiltLevels = 8.0;
constexpr size_t kMinTiltedPixels = 50;
constexpr double kMinDistinctWeight = 0.1;
constexpr BYTE kFlatNormalLow = 127;
constexpr BYTE kFlatNormalHigh = 128;
constexpr BYTE kFlatNormalUp = 255;
constexpr uint32_t kRippleForwardFlag = 0x1;
constexpr uint32_t kRippleWorldMs = 3600000;
constexpr double kRippleMsPerSecond = 1000.0;

WaterContact BasinContact(uint64_t guid, float x, float y, float depth)
{
    WaterContact contact;
    contact.guid = guid;
    const Vec3 p = Add({x, y, kWaterSurfaceZ - depth}, kGameLikeWorldOffset);
    contact.position[0] = p.x;
    contact.position[1] = p.y;
    contact.position[2] = p.z;
    contact.surface = kWaterSurfaceZ + kGameLikeWorldOffset.z;
    contact.radius = kRippleUnitRadius;
    contact.height = kRippleUnitHeight;
    contact.speed = kRippleUnitSpeed;
    return contact;
}

WaterContactFrame ContactsOf(std::initializer_list<WaterContact> contacts)
{
    WaterContactFrame frame;
    for (const WaterContact& contact : contacts)
        frame.contacts[frame.count++] = contact;
    return frame;
}

WaterFrameResult RenderRippleFrame(BasinClient& client, WaterView view, double seconds,
                                   const WaterContactFrame& contacts, const WaterFrame& frame = WaterFrame())
{
    vf_test_set_water_seconds(seconds);
    view.water.contacts = contacts;
    client.UseView(view);
    return client.Render(frame);
}

float RippleUnitY(int frame)
{
    return kRippleUnitStartY + kRippleUnitSpeed * static_cast<float>(frame * kRippleFrame);
}

WaterContact WadingUnit(int frame, double seconds, water_contact_checks::ClientRippleClock& clock)
{
    WaterContact unit = BasinContact(1, kRippleUnitX, RippleUnitY(frame), kRippleWadingDepth);
    unit.movementFlags = kRippleForwardFlag;
    unit.nextRippleMs =
        clock.Due(kRippleWorldMs + static_cast<uint32_t>(std::lround(seconds * kRippleMsPerSecond)), unit);
    return unit;
}

WaterFrameResult RunUnitThroughWater(BasinClient& client, const WaterView& view, double start,
                                     const WaterFrame& frame = WaterFrame())
{
    RenderRippleFrame(client, view, start, {}, frame);
    water_contact_checks::ClientRippleClock clock;
    WaterFrameResult last;
    for (int k = 1; k <= kRippleFrames; ++k)
    {
        const double seconds = start + k * kRippleFrame;
        last = RenderRippleFrame(client, view, seconds, ContactsOf({WadingUnit(k, seconds, clock)}), frame);
    }
    return last;
}

WaterRippleStats RippleStats()
{
    WaterRippleStats stats;
    vf_test_water_ripple_stats(&stats);
    return stats;
}

bool NearRipplePath(const WaterView& v, UINT x, UINT y)
{
    const Vec3 ray = PixelRay(v, x + 0.5f, y + 0.5f);
    if (ray.z >= 0.0f)
        return false;
    const float depth = ViewDepthOfPlane(v, ray, kWaterSurfaceZ);
    const float worldX = v.eye.x + ray.x * depth - kGameLikeWorldOffset.x;
    const float worldY = v.eye.y + ray.y * depth - kGameLikeWorldOffset.y;
    return worldX > kRippleUnitX - kRippleReach && worldX < kRippleUnitX + kRippleReach &&
           worldY > kRippleUnitStartY - kRippleReach && worldY < RippleUnitY(kRippleFrames) + kRippleReach;
}

struct PathComparison
{
    size_t nearPath = 0;
    size_t changedNearPath = 0;
    size_t changedElsewhere = 0;
};

PathComparison CompareAroundPath(const WaterView& v, const Image& a, const Image& b)
{
    PathComparison c;
    for (UINT y = 0; y < a.h; ++y)
        for (UINT x = 0; x < a.w; ++x)
        {
            const bool changed = std::memcmp(a.At(x, y), b.At(x, y), 3) != 0;
            if (NearRipplePath(v, x, y))
            {
                ++c.nearPath;
                c.changedNearPath += changed ? 1 : 0;
            }
            else
                c.changedElsewhere += changed ? 1 : 0;
        }
    return c;
}

Config RippleConfig(const Config& base, float ripples)
{
    Config cfg = WaterConfig(base);
    cfg.waterWaves = 0.0f;
    cfg.waterRipples = ripples;
    return cfg;
}

void ReleaseRipples(BasinClient& client, const Config& base, double seconds)
{
    const Config off = RippleConfig(base, 0.0f);
    vf_test_set_config(&off);
    RenderRippleFrame(client, client.View(), seconds, {});
}

void CheckRipplesChangeNormalsOnlyWhereTheyAre(BasinClient& client, const Config& base, const std::wstring& outDir)
{
    const WaterView view = client.View();
    Config normals = RippleConfig(base, 1.0f);
    normals.waterDebugView = kNormalDebugView;
    ReleaseRipples(client, base, kRippleStart);
    vf_test_set_config(&normals);
    const WaterFrameResult rippled = RunUnitThroughWater(client, view, kRippleStart);
    const WaterRippleStats stats = RippleStats();
    const double end = kRippleStart + kRippleFrames * kRippleFrame;
    Config flat = normals;
    flat.waterRipples = 0.0f;
    vf_test_set_config(&flat);
    const WaterFrameResult calm = RenderRippleFrame(client, view, end, {});
    SaveImage(outDir, L"water-ripple-normals", rippled.image);
    const PathComparison c = CompareAroundPath(view, rippled.image, calm.image);
    std::printf("     unit wading 1 s at %.0f yd/s: %llu steps, %u contacts; %zu of %zu pixels near its path changed, "
                "%zu elsewhere\n",
                kRippleUnitSpeed, static_cast<unsigned long long>(stats.steps), stats.contacts, c.changedNearPath,
                c.nearPath, c.changedElsewhere);
    Check(rippled.began && stats.shaded && stats.steps > 0 && c.changedNearPath >= kMinRipplePixels &&
              c.changedElsewhere == 0 && rippled.stateKept,
          "a unit wading through flat water bends the normals around its path and nowhere else, and the pass leaves "
          "the device state as it found it");

    Config debug = RippleConfig(base, 1.0f);
    debug.waterDebugView = kRippleDebugView;
    vf_test_set_config(&debug);
    const WaterFrameResult heights = RunUnitThroughWater(client, view, end + kRippleGap);
    SaveImage(outDir, L"water-debug-6", heights.image);
    size_t displaced = 0;
    for (UINT y = 0; y < heights.image.h; ++y)
        for (UINT x = 0; x < heights.image.w; ++x)
            if (NearRipplePath(view, x, y) &&
                std::abs(static_cast<int>(heights.image.At(x, y)[1]) - kFlatRippleGrey) > kRippleGreyTolerance)
                ++displaced;
    Check(heights.began && displaced >= kMinRipplePixels,
          "WaterDebugView 6 shows the ripple height around the unit's path");
}

struct RippleShadingReference
{
    WaterRippleShading constants;
    water_ripple_checks::RippleState map;

    double Height(double u, double v) const
    {
        const double s = u * map.texels - 0.5;
        const double t = v * map.texels - 0.5;
        const int x = static_cast<int>(std::floor(s));
        const int y = static_cast<int>(std::floor(t));
        const double fx = s - x;
        const double fy = t - y;
        return (1.0 - fy) * ((1.0 - fx) * Texel(x, y) + fx * Texel(x + 1, y)) +
               fy * ((1.0 - fx) * Texel(x, y + 1) + fx * Texel(x + 1, y + 1));
    }

private:
    double Texel(int x, int y) const
    {
        x = std::clamp(x, 0, map.texels - 1);
        y = std::clamp(y, 0, map.texels - 1);
        const double weight = constants.shape[0];
        return (1.0 - weight) * map.G(x, y) + weight * map.R(x, y);
    }
};

double FootprintTexels(const WaterView& v, const WaterRippleShading& c, UINT x, UINT y, Vec3 ray, float depth)
{
    const double rise = std::min(static_cast<double>(ray.z), -kMinRayRise);
    double longest = 0.0;
    for (const Vec3& next : {PixelRay(v, x + 1.5f, y + 0.5f), PixelRay(v, x + 0.5f, y + 1.5f)})
    {
        const Vec3 step = Sub(next, ray);
        const double fx = depth * (step.x - ray.x * step.z / rise);
        const double fy = depth * (step.y - ray.y * step.z / rise);
        longest = std::max(longest, std::sqrt(fx * fx + fy * fy));
    }
    return longest / c.fade[2];
}

double RippleCoverageAt(const WaterView& v, const WaterRippleShading& c, UINT x, UINT y, Vec3 ray, float depth,
                        double u, double w)
{
    const double fromCentre = std::max(std::fabs(u - 0.5), std::fabs(w - 0.5)) * c.shape[2];
    const double window = std::clamp((c.fade[0] - fromCentre) * c.fade[1], 0.0, 1.0);
    const double footprint = FootprintTexels(v, c, x, y, ray, depth);
    return window * std::clamp(1.0 - (footprint - kRippleFullDetailTexels) / kRippleDetailFadeTexels, 0.0, 1.0);
}

bool ExpectedRippleNormal(const WaterView& v, const RippleShadingReference& r, UINT x, UINT y, float depthScale,
                          double normal[3])
{
    const Vec3 ray = PixelRay(v, x + 0.5f, y + 0.5f);
    if (ray.z >= 0.0f)
        return false;
    const float depth = ViewDepthOfPlane(v, ray, kWaterSurfaceZ) * depthScale;
    const WaterRippleShading& c = r.constants;
    const double u = (static_cast<double>(v.eye.x + ray.x * depth) - c.window[0]) * c.window[2];
    const double w = (static_cast<double>(v.eye.y + ray.y * depth) - c.window[1]) * c.window[2];
    const double coverage = RippleCoverageAt(v, c, x, y, ray, depth, u, w);
    double slope[2] = {};
    if (c.shape[1] > 0.0f && coverage > 0.0)
    {
        const double step = c.window[3];
        const double height = r.Height(u, w);
        const double dx = r.Height(u + step, w) - height;
        const double dy = r.Height(u, w + step) - height;
        const double gain = kForeverRippleSlopeScale * c.shape[1] * coverage / std::sqrt((1 + dx * dx) * (1 + dy * dy));
        slope[0] = dx * gain;
        slope[1] = dy * gain;
    }
    const double length = std::sqrt(slope[0] * slope[0] + slope[1] * slope[1] + 1.0);
    normal[0] = -slope[0] / length;
    normal[1] = -slope[1] / length;
    normal[2] = 1.0 / length;
    return true;
}

bool FlatWaterNormal(const Image& image, UINT x, UINT y)
{
    const BYTE* p = image.At(x, y);
    return p[0] == kFlatNormalUp && p[1] >= kFlatNormalLow && p[1] <= kFlatNormalHigh && p[2] >= kFlatNormalLow &&
           p[2] <= kFlatNormalHigh;
}

struct NormalComparison
{
    size_t compared = 0;
    size_t tilted = 0;
    size_t outliers = 0;
    double worst = 0.0;
};

struct NormalRange
{
    double low[3] = {255.0, 255.0, 255.0};
    double high[3] = {};
    double tilt = 0.0;
};

bool ExpectedNormalRange(const WaterView& v, const RippleShadingReference& r, UINT x, UINT y, NormalRange& range)
{
    for (float depthScale : {1.0f - kDepthCopyPrecision, 1.0f, 1.0f + kDepthCopyPrecision})
    {
        double normal[3];
        if (!ExpectedRippleNormal(v, r, x, y, depthScale, normal))
            return false;
        for (int axis = 0; axis < 3; ++axis)
        {
            const double level = 255.0 * (0.5 + 0.5 * normal[axis]);
            range.low[axis] = std::min(range.low[axis], level);
            range.high[axis] = std::max(range.high[axis], level);
        }
        if (depthScale == 1.0f)
            range.tilt = 127.5 * std::max(std::fabs(normal[0]), std::fabs(normal[1]));
    }
    return true;
}

NormalComparison CompareRippleNormals(const WaterView& v, const RippleShadingReference& r, const Image& calm,
                                      const Image& rippled)
{
    NormalComparison c;
    for (UINT y = 0; y < rippled.h; ++y)
        for (UINT x = 0; x < rippled.w; ++x)
        {
            NormalRange range;
            if (!NearRipplePath(v, x, y) || !FlatWaterNormal(calm, x, y) || !ExpectedNormalRange(v, r, x, y, range))
                continue;
            const BYTE* p = rippled.At(x, y);
            const BYTE shaded[3] = {p[2], p[1], p[0]};
            double outside = 0.0;
            for (int axis = 0; axis < 3; ++axis)
                outside = std::max({outside, range.low[axis] - shaded[axis], shaded[axis] - range.high[axis]});
            c.worst = std::max(c.worst, outside);
            c.outliers += outside > kMaxNormalLevels ? 1 : 0;
            ++c.compared;
            c.tilted += range.tilt >= kMinTiltLevels ? 1 : 0;
        }
    return c;
}

void CheckRippleSlopeFollowsForever(Harness& h, BasinClient& client, const Config& base)
{
    const WaterView view = client.View();
    Config normals = RippleConfig(base, 0.0f);
    normals.waterDebugView = kNormalDebugView;
    vf_test_set_config(&normals);
    const WaterFrameResult calm = RenderRippleFrame(client, view, kRippleStart, {});
    normals.waterRipples = kSlopeCheckGain;
    vf_test_set_config(&normals);
    const double start = kRippleStart + kRippleGap;
    RunUnitThroughWater(client, view, start);
    const double between = start + kRippleFrames * kRippleFrame + kSlopeCheckStepFraction * kWaterRippleStepSeconds;
    const WaterFrameResult rippled = RenderRippleFrame(client, view, between, {});
    RippleShadingReference reference;
    vf_test_water_ripple_shading(&reference.constants);
    const bool read = water_ripple_checks::ReadRippleMap(h.dev, reference.constants.map, reference.map);
    const NormalComparison c = CompareRippleNormals(view, reference, calm.image, rippled.image);
    const double weight = reference.constants.shape[0];
    std::printf("     ripple slope at step fraction %.2f and WaterRipples %.1f: %zu water pixels near the path against "
                "the 7552035 formula over view depths within %.1f%%, %zu tilted by %.0f+ levels; %zu more than "
                "%.0f/255 outside it, worst %.2f/255\n",
                weight, reference.constants.shape[1], c.compared, kDepthCopyPrecision * 100.0f, c.tilted,
                kMinTiltLevels, c.outliers, kMaxNormalLevels, c.worst);
    Check(calm.began && rippled.began && read && std::fabs(weight - 0.5) >= kMinDistinctWeight &&
              weight >= kMinDistinctWeight && weight <= 1.0 - kMinDistinctWeight &&
              reference.constants.shape[1] == kSlopeCheckGain && c.tilted >= kMinTiltedPixels &&
              c.outliers <= kMaxOutlierShare * c.compared && c.worst <= kMaxOutlierNormalLevels,
          "the shaded ripple normals follow Forever's one-map slope: lerp(G, R, w) between steps, forward differences, "
          "(dx, dy)/sqrt((1 + dx^2)(1 + dy^2)) times 3 WaterRipples and the fade, to within the precision of the water "
          "depth copy except at most 0.5% of the compared pixels, none more than 6/255 off (the GPU's bilinear "
          "weight precision next to fresh impulses)");
    ReleaseRipples(client, base, between + kRippleGap);
}

void CheckRipplesOffRenderAsBefore(BasinClient& client, const Config& base)
{
    const WaterView view = client.View();
    ReleaseRipples(client, base, kRippleStart);
    const double seconds = kRippleStart + kRippleGap;
    WaterFrame hooked;
    hooked.calls = WaterCalls::Hooks;
    const WaterContactFrame contacts = ContactsOf({BasinContact(1, kRippleUnitX, 0.0f, kRippleDeepDepth)});
    const Config off = RippleConfig(base, 0.0f);
    vf_test_set_config(&off);
    const unsigned readsBefore = vf_test_water_contact_reads();
    RenderRippleFrame(client, view, seconds, {}, hooked);
    const WaterFrameResult withContacts = RenderRippleFrame(client, view, seconds, contacts, hooked);
    const unsigned readsOff = vf_test_water_contact_reads() - readsBefore;
    const unsigned heldOff = vf_test_water_resources_held();
    const Config on = RippleConfig(base, 1.0f);
    vf_test_set_config(&on);
    const WaterFrameResult idle = RenderRippleFrame(client, view, seconds, {}, hooked);
    const unsigned readsOn = vf_test_water_contact_reads() - readsBefore - readsOff;
    std::printf("     WaterRipples 0: %u contact reads, resources 0x%X; WaterRipples 1: %u reads; armed %d and %d\n",
                readsOff, heldOff, readsOn, withContacts.began, idle.began);
    Check(withContacts.began && idle.began && SameImage(withContacts.image, idle.image) && readsOff == 0 &&
              readsOn == 1 && !(heldOff & kRippleMapsHeld),
          "WaterRipples=0 reads no units, holds no ripple maps and shades the same image as idle ripples (the harness "
          "shows the renderer's image, not the game's)");
    ReleaseRipples(client, base, seconds);
}

WaterRippleStats StartRipples(BasinClient& client, const WaterView& view, double& seconds)
{
    RenderRippleFrame(client, view, seconds, {});
    const WaterContactFrame deep = ContactsOf({BasinContact(1, kRippleUnitX, 0.0f, kRippleDeepDepth)});
    for (int k = 0; k < 4; ++k)
    {
        seconds += kRippleFrame * 2.0;
        RenderRippleFrame(client, view, seconds, deep);
    }
    return RippleStats();
}

struct QualitySwitch
{
    WaterRippleStats switched;
    WaterRippleStats resumed;
    bool cleared = false;
};

QualitySwitch SwitchRippleQuality(Harness& h, BasinClient& client, const Config& on, int quality, double& seconds)
{
    const WaterContactFrame deep = ContactsOf({BasinContact(1, kRippleUnitX, 0.0f, kRippleDeepDepth)});
    Config switched = on;
    switched.waterQuality = quality;
    vf_test_set_config(&switched);
    QualitySwitch result;
    seconds += kRippleFrame * 2.0;
    RenderRippleFrame(client, client.View(), seconds, deep);
    result.switched = RippleStats();
    seconds += kRippleFrame * 3.0;
    RenderRippleFrame(client, client.View(), seconds, deep);
    result.resumed = RippleStats();
    WaterRippleShading shading;
    vf_test_water_ripple_shading(&shading);
    water_ripple_checks::RippleState map;
    result.cleared = water_ripple_checks::ReadRippleMap(h.dev, shading.map, map) &&
                     water_ripple_checks::MaxDifference(map, water_ripple_checks::ZeroState(map.texels)) == 0.0f;
    return result;
}

bool RestartedAtSize(const QualitySwitch& s, const WaterRippleStats& before, int texels)
{
    return s.switched.texels == texels && s.switched.running && !s.switched.shaded &&
           s.switched.restarts == before.restarts && s.resumed.texels == texels && s.resumed.shaded && s.cleared;
}

void CheckLiveQualitySwitchRestartsRipples(Harness& h, BasinClient& client, const Config& on, double& seconds)
{
    Config high = on;
    high.waterQuality = 2;
    vf_test_set_config(&high);
    seconds += kRippleGap;
    const WaterRippleStats started = StartRipples(client, client.View(), seconds);
    const QualitySwitch lower = SwitchRippleQuality(h, client, on, 1, seconds);
    const QualitySwitch higher = SwitchRippleQuality(h, client, on, 2, seconds);
    std::printf("     live WaterQuality 2 -> 1 -> 2: %d -> %d -> %d texels, shaded on the switch frames %d and %d, "
                "cleared maps %d and %d\n",
                started.texels, lower.switched.texels, higher.switched.texels, lower.switched.shaded,
                higher.switched.shaded, lower.cleared, higher.cleared);
    Check(started.shaded && started.texels == kWaterRippleTexels &&
              RestartedAtSize(lower, started, kWaterRippleTexelsLow) &&
              RestartedAtSize(higher, started, kWaterRippleTexels),
          "WaterQuality 1 simulates ripples on 256 x 256 texels and WaterQuality 2 on 512 x 512 (32 and 64 yd); "
          "switching it while ripples run restarts them on the new map at once, which is cleared by its first step "
          "before it is sampled");
}

void CheckRippleResets(Harness& h, BasinClient& client, const Config& base)
{
    const WaterView view = client.View();
    ReleaseRipples(client, base, kRippleStart);
    const Config on = RippleConfig(base, 1.0f);
    vf_test_set_config(&on);
    double seconds = kRippleStart;
    const WaterRippleStats entered = StartRipples(client, view, seconds);
    WaterView otherMap = view;
    otherMap.in.mapId = kOtherMap;
    seconds += kRippleFrame;
    RenderRippleFrame(client, otherMap, seconds, {});
    const WaterRippleStats afterMap = RippleStats();
    std::printf("     entry impulse: shaded %d after %llu steps; map change: shaded %d, restarts %u -> %u\n",
                entered.shaded, static_cast<unsigned long long>(entered.steps), afterMap.shaded, entered.restarts,
                afterMap.restarts);
    Check(entered.shaded && entered.running && !afterMap.shaded && !afterMap.running &&
              afterMap.restarts == entered.restarts + 1,
          "a unit jumping into the water starts the ripples and a map change clears them");

    seconds += kRippleFrame;
    const WaterRippleStats again = StartRipples(client, otherMap, seconds);
    seconds += kRippleGap;
    RenderRippleFrame(client, otherMap, seconds, {});
    const WaterRippleStats afterGap = RippleStats();
    Check(again.shaded && !afterGap.shaded && afterGap.restarts == again.restarts + 1,
          "a gap of more than a second without shaded water clears the ripples");

    seconds += kRippleFrame;
    const WaterRippleStats beforeReset = StartRipples(client, view, seconds);
    const unsigned heldBefore = vf_test_water_resources_held();
    h.ReleaseEngineObjects();
    const HRESULT reset = h.dev->Reset(&h.pp);
    h.CreateEngineObjects();
    const unsigned heldAfter = vf_test_water_resources_held();
    seconds += kRippleFrame;
    RenderRippleFrame(client, view, seconds, {});
    const WaterRippleStats afterReset = RippleStats();
    std::printf("     device Reset: ripple maps held %d -> %d, shaded afterwards %d\n",
                (heldBefore & kRippleMapsHeld) != 0, (heldAfter & kRippleMapsHeld) != 0,
                afterReset.shaded);
    Check(SUCCEEDED(reset) && beforeReset.shaded && (heldBefore & kRippleMapsHeld) &&
              !(heldAfter & kRippleMapsHeld) && !afterReset.shaded,
          "a device Reset releases the ripple maps and the next frame starts without ripples");

    CheckLiveQualitySwitchRestartsRipples(h, client, on, seconds);
    ReleaseRipples(client, base, seconds + kRippleGap);
}

void WarmUpGpuKeepingTheFrame(IDirect3DDevice9* dev)
{
    IDirect3DStateBlock9* state = nullptr;
    IDirect3DSurface9* target = nullptr;
    IDirect3DSurface9* depth = nullptr;
    dev->CreateStateBlock(D3DSBT_ALL, &state);
    dev->GetRenderTarget(0, &target);
    dev->GetDepthStencilSurface(&depth);
    water_fft_checks::WarmUpGpuClocks(dev);
    if (state)
    {
        state->Apply();
        state->Release();
    }
    dev->SetRenderTarget(0, target);
    dev->SetDepthStencilSurface(depth);
    for (IUnknown* held : {static_cast<IUnknown*>(target), static_cast<IUnknown*>(depth)})
        if (held)
            held->Release();
}

bool ParseWaterSummary(const std::string& text, float& medianMs, char (&details)[160])
{
    const size_t at = text.rfind("water gpu ");
    unsigned frames = 0;
    unsigned skipped = 0;
    return at != std::string::npos &&
           std::sscanf(text.c_str() + at, "water gpu %f ms (median of %u frames, %u skipped), %159[^\r\n]", &medianMs,
                       &frames, &skipped, details) == 4;
}

void CheckRippleSummaryAndCost(Harness& h, BasinClient& client, const Config& base)
{
    const WaterView view = client.View();
    ReleaseRipples(client, base, kRippleStart);
    Config logged = RippleConfig(base, 1.0f);
    logged.logLevel = static_cast<int>(LogLevel::Info);
    vf_test_set_config(&logged);
    double seconds = kRippleStart;
    float medians[2] = {};
    char details[2][160] = {};
    bool parsed = true;
    for (int active = 0; active < 2; ++active)
    {
        water_contact_checks::ClientRippleClock clock;
        WarmUpGpuKeepingTheFrame(h.dev);
        vf_test_force_water_summary();
        RenderRippleFrame(client, view, seconds, {});
        const size_t start = water_settings_checks::DllLogSize();
        for (int k = 1; k <= kRippleSummaryFrames; ++k)
        {
            seconds += kRippleFrame;
            const WaterContactFrame contacts =
                active ? ContactsOf({WadingUnit(k % kRippleFrames, seconds, clock)}) : WaterContactFrame();
            RenderRippleFrame(client, view, seconds, contacts);
        }
        vf_test_force_water_summary();
        seconds += kRippleFrame;
        RenderRippleFrame(client, view, seconds, {});
        parsed = ParseWaterSummary(runtime_cost::LogWrittenSince(start), medians[active], details[active]) && parsed;
    }
    std::printf("     water gpu at %lux%lu without contacts %.3f ms (%s); with a wading unit %.3f ms (%s)\n",
                view.world.Width, view.world.Height, medians[0], details[0], medians[1], details[1]);
    Check(parsed && std::strcmp(details[0], "classes lake, waves flat, ripples idle, up to 0 contacts") == 0 &&
              std::strcmp(details[1], "classes lake, waves flat, ripples 512 at 0.125 yd, 30 Hz, up to 1 contacts, "
                                      "0 steps dropped") == 0,
          "the water summary reports idle ripples, and the ripple map size, rate, contacts and dropped steps while a "
          "unit wades (the harness measures the renderer, not an in-game frame)");
    ReleaseRipples(client, base, seconds + kRippleGap);
}

void CheckWaterRipplePass(Harness& h, const std::wstring& outDir)
{
    Config base;
    vf_test_get_config(&base);
    BasinClient client(h, MakeWaterView(kRippleEye, kRippleEyeTarget));
    Check(AssignWaterData(MakeSyntheticWaterData()), "synthetic water data assigned for the ripple pass");
    CheckRipplesChangeNormalsOnlyWhereTheyAre(client, base, outDir);
    CheckRippleSlopeFollowsForever(h, client, base);
    CheckRipplesOffRenderAsBefore(client, base);
    CheckRippleResets(h, client, base);
    CheckRippleSummaryAndCost(h, client, base);
    vf_test_set_water_seconds(kRealTime);
    vf_test_set_config(&base);
}
}
