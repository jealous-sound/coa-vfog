#pragma once

namespace ripple_scene
{
using water_checks::BasinClient;
using water_checks::kBasinFloorZ;
using water_checks::kBeachRisePerYard;
using water_checks::kBeachStartX;
using water_checks::kWaterSurfaceZ;
using water_checks::WaterFrameResult;
using water_checks::WaterView;

constexpr float kShinDepth = 0.5f;
constexpr float kWaistDepth = 1.0f;
constexpr float kSwimDepth = 1.5f;
constexpr float kRunSpeed = 7.0f;
constexpr float kWalkSpeed = 2.5f;
constexpr float kSwimSpeed = 4.72f;
constexpr float kUnitHeight = 2.0f;
constexpr float kUnitRadius = 0.35f;
constexpr float kUnitHalfWidth = 0.3f;
constexpr float kUnitTopAboveSurface = 1.2f;
constexpr float kSwimmerHalfLength = 0.9f;
constexpr float kSwimmerBelowSurface = 0.3f;
constexpr float kSwimmerAboveSurface = 0.2f;
constexpr DWORD kUnitColour = 0xFF3A2E28;
constexpr DWORD kNpcColour = 0xFF5A3A5A;
constexpr float kCameraBehind = 7.0f;
constexpr float kCameraAbove = 4.5f;
constexpr float kLookAtAbove = 1.0f;
constexpr float kStartY = -20.0f;
constexpr float kCrossingStartY = -12.0f;
constexpr float kNpcLaneBeforePlayer = 2.5f;
constexpr float kShallowsDepth = 0.15f;
constexpr double kWadeInStandSeconds = 1.0;
constexpr double kWadeInSeconds = 4.0;
constexpr double kStartSeconds = 60.0;
constexpr double kFrameSeconds = 1.0 / 60.0;
constexpr double kCheckFrameSeconds = 1.0 / 30.0;
constexpr double kStandSeconds = 3.0;
constexpr double kMoveSeconds = 3.0;
constexpr double kStandAfterSeconds = 2.0;
constexpr UINT kSceneWidth = 1280;
constexpr UINT kSceneHeight = 720;
constexpr uint64_t kPlayerGuid = 1;
constexpr uint64_t kSwimmerGuid = 2;
constexpr uint64_t kNpcGuid = 3;
constexpr int kNormalDebugView = 1;
constexpr float kUnseenRippleGain = 1e-6f;
constexpr float kPebbleSize = 0.125f;
constexpr float kPebbleFieldNearX = 360.0f;
constexpr float kPebbleFieldFarX = 400.0f;
constexpr float kPebbleFieldStartY = -30.0f;
constexpr float kPebbleFieldEndY = 20.0f;
constexpr float kPebbleLift = 0.01f;
constexpr float kPebbleShadeSpread = 0.15f;
constexpr uint32_t kPebbleHashMultiplier = 0x9E3779B1u;
constexpr uint32_t kPebbleHashShift = 15;
constexpr uint32_t kPebbleShadeLevels = 256;
constexpr double kVisibleRippleLevels = 8.0;
constexpr double kLumaLevels = 255.0;
constexpr float kFeetYards = 1.5f;
constexpr float kViewedYards = 12.0f;
constexpr float kWedgeNearestBehind = 1.0f;
constexpr float kWedgeFarthestBehind = 8.0f;
constexpr float kWedgeMargin = 0.75f;
constexpr double kMaxIdleMotion = 1e-3;
constexpr double kMinWedgeShare = 0.1;
constexpr double kMinSideShare = 0.05;
constexpr double kMaxStrayShare = 5e-4;
constexpr float kWakeRipples = 0.5f;

struct Mover
{
    uint64_t guid;
    float startX;
    float startY;
    float directionX;
    float directionY;
    float speed;
    bool swimming;
    double standSeconds;
    double moveSeconds;
    DWORD colour;
};

enum class CameraRig
{
    BehindFirstMover,
    AcrossTheLanes,
};

struct Shot
{
    const wchar_t* name;
    std::vector<Mover> movers;
    CameraRig rig;
    double seconds;
    std::vector<double> captures;
};

float BeachXAtDepth(float depth)
{
    return kBeachStartX + (kWaterSurfaceZ - depth - kBasinFloorZ) / kBeachRisePerYard;
}

float BeachFloorZ(float x)
{
    return kBasinFloorZ + (x - kBeachStartX) * kBeachRisePerYard + kPebbleLift;
}

float WaterDepthAt(float x)
{
    const float floor = x < kBeachStartX ? kBasinFloorZ : kBasinFloorZ + (x - kBeachStartX) * kBeachRisePerYard;
    return std::max(0.0f, kWaterSurfaceZ - floor);
}

Mover Wader(float depth, float speed)
{
    return {kPlayerGuid, BeachXAtDepth(depth), kStartY, 0.0f, 1.0f, speed, false, kStandSeconds, kMoveSeconds,
            kUnitColour};
}

Mover Crossing(uint64_t guid, float x, float speed, bool swimming, double standSeconds, DWORD colour)
{
    return {guid, x, kCrossingStartY, 0.0f, 1.0f, speed, swimming, standSeconds, 2.0 * -kCrossingStartY / speed,
            colour};
}

std::vector<Shot> Shots()
{
    const double timeline = kStandSeconds + kMoveSeconds + kStandAfterSeconds;
    const std::vector<double> timelineCaptures = {1.0, 2.9, 3.4, 4.5, 6.0, 6.4, 7.0, 8.0};
    const float playerX = BeachXAtDepth(kShinDepth);
    const float npcX = playerX - kNpcLaneBeforePlayer;
    const Mover standing = {kPlayerGuid, playerX, 0.0f, 0.0f, 1.0f, 0.0f, false, 0.0, 0.0, kUnitColour};
    const Mover swimmer = Crossing(kSwimmerGuid, BeachXAtDepth(kSwimDepth), kSwimSpeed, true, 0.0, kUnitColour);
    const Mover npcWalking = Crossing(kNpcGuid, npcX, kWalkSpeed, false, 1.0, kNpcColour);
    const Mover npcRunning = Crossing(kNpcGuid, npcX, kRunSpeed, false, 1.0, kNpcColour);
    const Mover wadingIn = {kPlayerGuid, BeachXAtDepth(kShallowsDepth), 0.0f, -1.0f, 0.0f, kRunSpeed, false,
                            kWadeInStandSeconds, kWadeInSeconds, kUnitColour};
    return {
        {L"wader-shin", {Wader(kShinDepth, kRunSpeed)}, CameraRig::BehindFirstMover, timeline, timelineCaptures},
        {L"wader-waist", {Wader(kWaistDepth, kRunSpeed)}, CameraRig::BehindFirstMover, timeline, timelineCaptures},
        {L"walker-shin", {Wader(kShinDepth, kWalkSpeed)}, CameraRig::BehindFirstMover, timeline, timelineCaptures},
        {L"swimmer", {swimmer}, CameraRig::AcrossTheLanes, swimmer.moveSeconds + 1.0, {1.0, 2.5, 4.0, 5.0, 6.0}},
        {L"npc-walk", {standing, npcWalking}, CameraRig::AcrossTheLanes, npcWalking.moveSeconds + 2.0,
         {0.9, 3.0, 5.8, 8.0, 10.6}},
        {L"npc-run", {standing, npcRunning}, CameraRig::AcrossTheLanes, npcRunning.moveSeconds + 3.0,
         {0.9, 1.9, 2.7, 3.4, 4.4, 6.4}},
        {L"wade-in", {wadingIn}, CameraRig::BehindFirstMover, kWadeInStandSeconds + kWadeInSeconds + 2.0,
         {0.9, 2.5, 4.1, 4.4, 4.8, 5.3, 6.0, 7.0}},
    };
}

double Travelled(const Mover& m, double seconds)
{
    return m.speed * std::clamp(seconds - m.standSeconds, 0.0, m.moveSeconds);
}

bool Moving(const Mover& m, double seconds)
{
    return m.speed > 0.0f && seconds > m.standSeconds && seconds <= m.standSeconds + m.moveSeconds;
}

Vec3 FeetAt(const Mover& m, double seconds)
{
    const float travelled = static_cast<float>(Travelled(m, seconds));
    const float x = m.startX + m.directionX * travelled;
    const float depth = m.swimming ? kSwimDepth : WaterDepthAt(x);
    return {x, m.startY + m.directionY * travelled, kWaterSurfaceZ - depth};
}

bool MovesAlongY(const Mover& m)
{
    return m.directionX == 0.0f && m.directionY == 1.0f;
}

DWORD PebbleColour(int column, int row)
{
    uint32_t h = static_cast<uint32_t>(column) * kPebbleHashMultiplier ^ static_cast<uint32_t>(row);
    h = (h ^ (h >> kPebbleHashShift)) * kPebbleHashMultiplier;
    h ^= h >> kPebbleHashShift;
    const float shade = 1.0f - kPebbleShadeSpread + 2.0f * kPebbleShadeSpread * (h % kPebbleShadeLevels) /
                                                         static_cast<float>(kPebbleShadeLevels - 1);
    auto channel = [shade](int shift) {
        const float value = ((water_checks::kFloorColour >> shift) & 0xFF) * shade;
        return static_cast<DWORD>(std::min(value, 255.0f)) << shift;
    };
    return 0xFF000000u | channel(16) | channel(8) | channel(0);
}

void AddPebbleField(std::vector<SceneVertex>& scene)
{
    const int columns = static_cast<int>((kPebbleFieldFarX - kPebbleFieldNearX) / kPebbleSize);
    const int rows = static_cast<int>((kPebbleFieldEndY - kPebbleFieldStartY) / kPebbleSize);
    for (int column = 0; column < columns; ++column)
        for (int row = 0; row < rows; ++row)
        {
            const float x0 = kPebbleFieldNearX + column * kPebbleSize;
            const float x1 = x0 + kPebbleSize;
            const float y0 = kPebbleFieldStartY + row * kPebbleSize;
            const float y1 = y0 + kPebbleSize;
            AddQuad(scene, {x0, y0, BeachFloorZ(x0)}, {x1, y0, BeachFloorZ(x1)}, {x1, y1, BeachFloorZ(x1)},
                    {x0, y1, BeachFloorZ(x0)}, PebbleColour(column, row));
        }
}

const std::vector<SceneVertex>& PebbledBasin()
{
    static const std::vector<SceneVertex> basin = [] {
        std::vector<SceneVertex> scene = water_checks::BuildBasinScene();
        AddPebbleField(scene);
        return scene;
    }();
    return basin;
}

void AddMoverBody(std::vector<SceneVertex>& scene, const Mover& m, double seconds)
{
    const Vec3 feet = FeetAt(m, seconds);
    if (m.swimming)
        AddBox(scene,
               {feet.x - kUnitHalfWidth, feet.y - 2.0f * kSwimmerHalfLength, kWaterSurfaceZ - kSwimmerBelowSurface},
               {feet.x + kUnitHalfWidth, feet.y, kWaterSurfaceZ + kSwimmerAboveSurface}, m.colour);
    else
        AddBox(scene, {feet.x - kUnitHalfWidth, feet.y - kUnitHalfWidth, feet.z},
               {feet.x + kUnitHalfWidth, feet.y + kUnitHalfWidth, kWaterSurfaceZ + kUnitTopAboveSurface}, m.colour);
}

WaterContact ContactOf(const Mover& m, double seconds)
{
    WaterContact contact;
    contact.guid = m.guid;
    const Vec3 feet = Add(FeetAt(m, seconds), kGameLikeWorldOffset);
    contact.position[0] = feet.x;
    contact.position[1] = feet.y;
    contact.position[2] = feet.z;
    contact.surface = kWaterSurfaceZ + kGameLikeWorldOffset.z;
    contact.radius = kUnitRadius;
    contact.height = kUnitHeight;
    contact.swimming = m.swimming;
    return contact;
}

WaterView ShotView(const Shot& shot, double seconds)
{
    const Mover& first = shot.movers.front();
    Vec3 eye;
    Vec3 at;
    if (shot.rig == CameraRig::BehindFirstMover)
    {
        const Vec3 feet = FeetAt(first, seconds);
        eye = {feet.x - first.directionX * kCameraBehind, feet.y - first.directionY * kCameraBehind,
               kWaterSurfaceZ + kCameraAbove};
        at = {feet.x, feet.y, kWaterSurfaceZ + kLookAtAbove};
    }
    else
    {
        eye = {first.startX - kCameraBehind, 0.0f, kWaterSurfaceZ + kCameraAbove};
        at = {first.startX, 0.0f, kWaterSurfaceZ + kLookAtAbove};
    }
    WaterView view = water_checks::MakeWaterView(eye, at);
    const Vec3 target = Add({at.x, at.y, kWaterSurfaceZ}, kGameLikeWorldOffset);
    view.in.camTarget[0] = target.x;
    view.in.camTarget[1] = target.y;
    view.in.camTarget[2] = target.z;
    return view;
}

WaterFrameResult RenderShotFrame(BasinClient& client, const Shot& shot, double seconds, bool inWater)
{
    std::vector<SceneVertex> scene = PebbledBasin();
    WaterContactFrame contacts;
    for (const Mover& m : shot.movers)
    {
        AddMoverBody(scene, m, seconds);
        if (inWater)
            contacts.contacts[contacts.count++] = ContactOf(m, seconds);
    }
    client.UseScene(std::move(scene));
    water_checks::WaterFrame frame;
    frame.otherPass = false;
    return water_checks::RenderRippleFrame(client, ShotView(shot, seconds), kStartSeconds + seconds, contacts, frame);
}

bool WaterHit(const WaterView& view, UINT x, UINT y, float& hitX, float& hitY)
{
    const Vec3 ray = water_checks::PixelRay(view, x + 0.5f, y + 0.5f);
    if (ray.z >= 0.0f)
        return false;
    const float depth = water_checks::ViewDepthOfPlane(view, ray, kWaterSurfaceZ);
    hitX = view.eye.x + ray.x * depth - kGameLikeWorldOffset.x;
    hitY = view.eye.y + ray.y * depth - kGameLikeWorldOffset.y;
    return true;
}

double LumaChange(const Image& a, const Image& b, UINT x, UINT y)
{
    return std::fabs(a.Luma(x, y) - b.Luma(x, y)) * kLumaLevels;
}

double MachHalfAngle(float speed)
{
    return water_ripple_checks::MachDegrees(speed) * kPi / 180.0;
}

bool OutrunsItsRipples(const Mover& m)
{
    return m.speed > water_ripple_checks::RippleSpeed();
}

struct CaptureMetrics
{
    double seconds = 0.0;
    size_t viewed = 0;
    size_t atFeet = 0;
    size_t waves = 0;
    size_t wedge[2] = {};
    size_t wedgeChanged[2] = {};
    size_t ahead = 0;
    float motion = 0.0f;

    size_t Wedge() const { return wedge[0] + wedge[1]; }
    size_t WedgeChanged() const { return wedgeChanged[0] + wedgeChanged[1]; }
    double Share(size_t part, size_t whole) const { return whole ? static_cast<double>(part) / whole : 0.0; }
    double WedgeShare() const { return Share(WedgeChanged(), Wedge()); }
    double SideShare(int side) const { return Share(wedgeChanged[side], wedge[side]); }
    double WaveShare() const { return Share(waves, viewed); }
    double AheadShare() const { return Share(ahead, viewed); }
    bool mapRead = false;
    bool measuredV = false;
    water_ripple_checks::WakeLines lines;
};

bool InWedge(float lateral, float behind, float halfAngle)
{
    if (behind < kWedgeNearestBehind || behind > kWedgeFarthestBehind)
        return false;
    const float arm = behind * std::tan(halfAngle);
    return std::fabs(lateral) <= arm + kWedgeMargin;
}

struct Heading
{
    float behind;
    float lateral;
};

Heading HeadingOf(const Mover& m, const Vec3& feet, float x, float y)
{
    const float dx = x - feet.x;
    const float dy = y - feet.y;
    return {-(dx * m.directionX + dy * m.directionY), dx * m.directionY - dy * m.directionX};
}

CaptureMetrics MeasureCapture(Harness& h, const Shot& shot, double seconds, const Image& rippled, const Image& calm)
{
    CaptureMetrics c;
    c.seconds = seconds;
    const WaterView view = ShotView(shot, seconds);
    const Mover* moving = nullptr;
    for (const Mover& m : shot.movers)
        if (Moving(m, seconds))
            moving = &m;
    const Vec3 movingFeet = moving ? FeetAt(*moving, seconds) : Vec3{};
    const double halfAngle = moving ? MachHalfAngle(moving->speed) : 0.0;
    for (UINT y = 0; y < view.world.Height; ++y)
        for (UINT x = 0; x < view.world.Width; ++x)
        {
            float hitX = 0.0f;
            float hitY = 0.0f;
            if (!WaterHit(view, x, y, hitX, hitY))
                continue;
            float nearest = std::numeric_limits<float>::infinity();
            for (const Mover& m : shot.movers)
            {
                const Vec3 feet = FeetAt(m, seconds);
                nearest = std::min(nearest, std::hypot(hitX - feet.x, hitY - feet.y));
            }
            if (nearest > kViewedYards)
                continue;
            ++c.viewed;
            const bool changed = LumaChange(rippled, calm, x, y) >= kVisibleRippleLevels;
            (nearest <= kFeetYards ? c.atFeet : c.waves) += changed ? 1 : 0;
            if (!moving || nearest <= kFeetYards)
                continue;
            const Heading heading = HeadingOf(*moving, movingFeet, hitX, hitY);
            if (InWedge(heading.lateral, heading.behind, static_cast<float>(halfAngle)))
            {
                const int side = heading.lateral < 0.0f ? 0 : 1;
                ++c.wedge[side];
                c.wedgeChanged[side] += changed ? 1 : 0;
            }
            c.ahead += changed && -heading.behind > kFeetYards ? 1 : 0;
        }
    WaterRippleShading shading;
    vf_test_water_ripple_shading(&shading);
    water_ripple_checks::RippleState map;
    c.mapRead = shading.map && water_ripple_checks::ReadRippleMap(h.dev, shading.map, map);
    if (!c.mapRead)
        return c;
    for (size_t i = 0; i < map.r.size(); ++i)
        c.motion = std::max(c.motion, std::fabs(map.r[i] - map.g[i]));
    if (moving && OutrunsItsRipples(*moving) && MovesAlongY(*moving))
    {
        const Vec3 world = Add(movingFeet, kGameLikeWorldOffset);
        const float px = (world.x - shading.window[0]) / kWaterRippleTexelYards;
        const float py = (world.y - shading.window[1]) / kWaterRippleTexelYards;
        c.lines = water_ripple_checks::MeasureWake(map, px, py);
        c.measuredV = true;
    }
    return c;
}

std::wstring CaptureName(const Shot& shot, double seconds)
{
    wchar_t name[96];
    std::swprintf(name, 96, L"%ls-%04.1fs", shot.name, seconds);
    return name;
}

void PrintMetrics(const Shot& shot, const CaptureMetrics& c)
{
    std::printf("     %ls %4.1f s: %zu water px within %.0f yd; changed by %.0f+ levels: %zu within %.1f yd of a unit, "
                "%zu beyond; map |R - G| %.2e",
                shot.name, c.seconds, c.viewed, kViewedYards, kVisibleRippleLevels, c.atFeet, kFeetYards, c.waves,
                c.motion);
    if (c.Wedge())
        std::printf("; V wedge %zu of %zu (%.1f%%; left %.1f%%, right %.1f%%), %zu ahead", c.WedgeChanged(), c.Wedge(),
                    100.0 * c.WedgeShare(), 100.0 * c.SideShare(0), 100.0 * c.SideShare(1), c.ahead);
    if (c.measuredV)
        std::printf("; map V front %.1f deg, brightest %.1f deg", c.lines.frontDegrees, c.lines.brightDegrees);
    std::printf("\n");
}

Config WakeConfig(const Config& base)
{
    Config cfg = water_checks::WaterConfig(base);
    cfg.waterRipples = kWakeRipples;
    return cfg;
}

struct ShotResult
{
    bool shaded = true;
    std::vector<CaptureMetrics> metrics;
};

ShotResult RenderShot(Harness& h, BasinClient& client, const Shot& shot, const Config& cfg, const std::wstring* outDir,
                      double frameSeconds = kFrameSeconds)
{
    Config off = cfg;
    off.waterRipples = 0.0f;
    vf_test_set_config(&off);
    RenderShotFrame(client, shot, 0.0, false);
    vf_test_set_config(&cfg);
    ShotResult result;
    size_t next = 0;
    for (int frame = 0; next < shot.captures.size(); ++frame)
    {
        const double seconds = frame * frameSeconds;
        const WaterFrameResult rendered = RenderShotFrame(client, shot, seconds, true);
        result.shaded = result.shaded && rendered.began;
        if (seconds + frameSeconds * 0.5 < shot.captures[next])
            continue;
        Config calm = cfg;
        calm.waterRipples = kUnseenRippleGain;
        vf_test_set_config(&calm);
        const Image calmImage = RenderShotFrame(client, shot, seconds, true).image;
        vf_test_set_config(&cfg);
        const WaterFrameResult again = RenderShotFrame(client, shot, seconds, true);
        const CaptureMetrics metrics = MeasureCapture(h, shot, seconds, again.image, calmImage);
        PrintMetrics(shot, metrics);
        result.metrics.push_back(metrics);
        if (outDir)
        {
            const std::wstring name = CaptureName(shot, shot.captures[next]);
            water_checks::SaveImage(*outDir, name.c_str(), again.image);
            water_checks::SaveImage(*outDir, (name + L"-calm").c_str(), calmImage);
            Config normals = cfg;
            normals.waterDebugView = kNormalDebugView;
            vf_test_set_config(&normals);
            water_checks::SaveImage(*outDir, (name + L"-normals").c_str(),
                                    RenderShotFrame(client, shot, seconds, true).image);
            vf_test_set_config(&cfg);
        }
        ++next;
    }
    return result;
}

void CheckWakesShowInShadedWater(Harness& h, const std::wstring& outDir, const std::string& waterDataPath)
{
    Config base;
    vf_test_get_config(&base);
    const bool loaded = vf_test_load_water_data(waterDataPath.c_str()) != 0;
    const Config cfg = WakeConfig(base);
    const std::vector<Shot> shots = Shots();
    BasinClient client(h, ShotView(shots[0], 0.0));
    bool idleCalm = loaded;
    bool wakeShows = loaded;
    for (const Shot* shot : {&shots[0], &shots[1]})
    {
        Shot checked = *shot;
        checked.captures = {kStandSeconds - 0.1, kStandSeconds + 1.5};
        const ShotResult result = RenderShot(h, client, checked, cfg, nullptr, kCheckFrameSeconds);
        if (result.metrics.size() != 2)
        {
            idleCalm = wakeShows = false;
            continue;
        }
        const CaptureMetrics& idle = result.metrics[0];
        const CaptureMetrics& running = result.metrics[1];
        idleCalm = idleCalm && result.shaded && idle.mapRead && idle.WaveShare() <= kMaxStrayShare &&
                   idle.motion <= kMaxIdleMotion;
        wakeShows = wakeShows && running.WedgeShare() >= kMinWedgeShare && running.SideShare(0) >= kMinSideShare &&
                    running.SideShare(1) >= kMinSideShare && running.AheadShare() <= kMaxStrayShare;
    }
    Check(idleCalm, "a unit standing shin- or waist-deep in shaded water at WaterRipples 0.5 leaves the ripple map "
                    "still once its footprint has settled (no texel changes by more than 0.001 in a step) and changes "
                    "at most 0.05% of the water pixels within 12 yd but farther than 1.5 yd from it by 8+ levels (a "
                    "harness pebble floor and sky, not the in-game look)");
    Check(wakeShows, "a unit running at 7 yd/s through shin- and waist-deep shaded water at WaterRipples 0.5 shows a V "
                     "behind it: 10%+ of the water inside asin(2.65 / 7) + 0.75 yd between 1 and 8 yd behind changes "
                     "by 8+ levels, 5%+ on each side of its path, and at most 0.05% of the water more than 1.5 yd "
                     "ahead of it");
    vf_test_set_water_seconds(water_checks::kRealTime);
    vf_test_set_config(&base);
    Check(water_checks::AssignWaterData(water_checks::MakeSyntheticWaterData()),
          "synthetic water data restored after the wake visibility check");
}

bool CreateSceneDevice(Harness& h, const wchar_t* windowClass)
{
    h.window = CreateWindowW(windowClass, L"vfog ripples", WS_OVERLAPPEDWINDOW, 0, 0, kSceneWidth, kSceneHeight,
                             nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    HMODULE d3d9 = LoadLibraryA("d3d9.dll");
    const auto create =
        d3d9 ? reinterpret_cast<IDirect3D9*(WINAPI*)(UINT)>(GetProcAddress(d3d9, "Direct3DCreate9")) : nullptr;
    if (!h.window || !create)
        return false;
    h.d3d = vf_test_wrap_direct3d9(create, D3D_SDK_VERSION);
    if (!h.d3d)
        return false;
    h.pp.Windowed = TRUE;
    h.pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    h.pp.BackBufferWidth = kSceneWidth;
    h.pp.BackBufferHeight = kSceneHeight;
    h.pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    h.pp.EnableAutoDepthStencil = TRUE;
    h.pp.AutoDepthStencilFormat = kClientDepthFormat;
    h.pp.hDeviceWindow = h.window;
    h.pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    const DWORD flags = D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE | D3DCREATE_FPU_PRESERVE;
    if (FAILED(h.d3d->CreateDevice(0, D3DDEVTYPE_HAL, h.window, flags, &h.pp, &h.dev)) || !h.dev)
        return false;
    h.CreateEngineObjects();
    return true;
}

int RunRippleScene(const std::wstring& outDir, const std::string& waterDataPath)
{
    CreateDirectoryW(outDir.c_str(), nullptr);
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    WNDCLASSW wc = {};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"vfog_ripples";
    RegisterClassW(&wc);
    Config startup;
    startup.overlay = false;
    vf_test_set_config(&startup);
    Harness h;
    bool ok = CreateSceneDevice(h, wc.lpszClassName) && vf_test_load_water_data(waterDataPath.c_str()) != 0;
    if (ok)
    {
        const Config cfg = WakeConfig(startup);
        const std::vector<Shot> shots = Shots();
        BasinClient client(h, ShotView(shots[0], 0.0));
        for (const Shot& shot : shots)
        {
            const ShotResult result = RenderShot(h, client, shot, cfg, &outDir);
            ok = ok && result.shaded && result.metrics.size() == shot.captures.size();
            std::printf("%ls: %s, %zu captures\n", shot.name, result.shaded ? "shaded" : "not shaded",
                        result.metrics.size());
        }
        vf_test_set_water_seconds(water_checks::kRealTime);
    }
    h.ReleaseEngineObjects();
    if (h.dev)
        h.dev->Release();
    if (h.d3d)
        h.d3d->Release();
    if (h.window)
        DestroyWindow(h.window);
    CoUninitialize();
    std::printf("ripple scene %s, written to %ls\n", ok ? "rendered" : "failed", outDir.c_str());
    return ok ? 0 : 1;
}
}
