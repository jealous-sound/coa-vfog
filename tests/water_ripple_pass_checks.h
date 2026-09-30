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

WaterFrameResult RunUnitThroughWater(BasinClient& client, const WaterView& view, double start,
                                     const WaterFrame& frame = WaterFrame())
{
    RenderRippleFrame(client, view, start, {}, frame);
    WaterFrameResult last;
    for (int k = 1; k <= kRippleFrames; ++k)
    {
        const WaterContact unit = BasinContact(1, kRippleUnitX, RippleUnitY(k), kRippleWadingDepth);
        last = RenderRippleFrame(client, view, start + k * kRippleFrame, ContactsOf({unit}), frame);
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

    int texels[2] = {};
    const int qualities[2] = {1, 2};
    for (int i = 0; i < 2; ++i)
    {
        Config quality = on;
        quality.waterQuality = qualities[i];
        vf_test_set_config(&quality);
        seconds += kRippleGap;
        StartRipples(client, view, seconds);
        texels[i] = RippleStats().texels;
    }
    Check(texels[0] == kWaterRippleTexelsLow && texels[1] == kWaterRippleTexels,
          "WaterQuality 1 simulates ripples on 256 x 256 texels and WaterQuality 2 on 512 x 512 (32 and 64 yd)");
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
        WarmUpGpuKeepingTheFrame(h.dev);
        vf_test_force_water_summary();
        RenderRippleFrame(client, view, seconds, {});
        const size_t start = water_settings_checks::DllLogSize();
        for (int k = 1; k <= kRippleSummaryFrames; ++k)
        {
            seconds += kRippleFrame;
            const WaterContactFrame contacts =
                active ? ContactsOf({BasinContact(1, kRippleUnitX, RippleUnitY(k % kRippleFrames), kRippleWadingDepth)})
                       : WaterContactFrame();
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
    CheckRipplesOffRenderAsBefore(client, base);
    CheckRippleResets(h, client, base);
    CheckRippleSummaryAndCost(h, client, base);
    vf_test_set_water_seconds(kRealTime);
    vf_test_set_config(&base);
}
}
