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
constexpr float kRunSpeed = 7.0f;
constexpr float kWalkSpeed = 2.5f;
constexpr float kUnitHeight = 2.0f;
constexpr float kUnitRadius = 0.35f;
constexpr float kUnitHalfWidth = 0.3f;
constexpr float kUnitTopAboveSurface = 1.2f;
constexpr DWORD kUnitColour = 0xFF3A2E28;
constexpr float kCameraBehind = 7.0f;
constexpr float kCameraAbove = 4.5f;
constexpr float kLookAtAbove = 1.0f;
constexpr float kStartY = -20.0f;
constexpr double kStartSeconds = 60.0;
constexpr double kFrameSeconds = 1.0 / 60.0;
constexpr double kCaptureSeconds[] = {0.5, 1.5, 3.0};
constexpr int kCaptureCount = sizeof(kCaptureSeconds) / sizeof(kCaptureSeconds[0]);
constexpr UINT kSceneWidth = 1280;
constexpr UINT kSceneHeight = 720;
constexpr uint64_t kWalkerGuid = 1;
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
constexpr uint32_t kWalkerForwardFlag = 0x1;
constexpr uint32_t kWorldStartMs = 7200000;
constexpr double kMsPerSecond = 1000.0;

struct Walk
{
    const wchar_t* name;
    float depth;
    float speed;
};

const Walk kWalks[] = {
    {L"shin-run", kShinDepth, kRunSpeed},
    {L"waist-run", kWaistDepth, kRunSpeed},
    {L"shin-walk", kShinDepth, kWalkSpeed},
};

float BeachXAtDepth(float depth)
{
    return kBeachStartX + (kWaterSurfaceZ - depth - kBasinFloorZ) / kBeachRisePerYard;
}

float BeachFloorZ(float x)
{
    return kBasinFloorZ + (x - kBeachStartX) * kBeachRisePerYard + kPebbleLift;
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

Vec3 FeetAt(const Walk& walk, double seconds)
{
    return {BeachXAtDepth(walk.depth), kStartY + walk.speed * static_cast<float>(seconds),
            kWaterSurfaceZ - walk.depth};
}

uint32_t WorldMs(double seconds)
{
    return kWorldStartMs + static_cast<uint32_t>(std::lround(seconds * kMsPerSecond));
}

WaterContact WalkerContact(const Walk& walk, double seconds, water_contact_checks::ClientRippleClock& clock)
{
    WaterContact contact;
    contact.guid = kWalkerGuid;
    const Vec3 feet = Add(FeetAt(walk, seconds), kGameLikeWorldOffset);
    contact.position[0] = feet.x;
    contact.position[1] = feet.y;
    contact.position[2] = feet.z;
    contact.surface = kWaterSurfaceZ + kGameLikeWorldOffset.z;
    contact.radius = kUnitRadius;
    contact.height = kUnitHeight;
    contact.speed = walk.speed;
    contact.movementFlags = kWalkerForwardFlag;
    contact.nextRippleMs = clock.Due(WorldMs(seconds), contact);
    return contact;
}

WaterView ThirdPersonView(const Walk& walk, double seconds)
{
    const Vec3 feet = FeetAt(walk, seconds);
    const Vec3 eye = {feet.x, feet.y - kCameraBehind, kWaterSurfaceZ + kCameraAbove};
    const Vec3 at = {feet.x, feet.y, kWaterSurfaceZ + kLookAtAbove};
    WaterView view = water_checks::MakeWaterView(eye, at);
    const Vec3 target = Add({feet.x, feet.y, kWaterSurfaceZ}, kGameLikeWorldOffset);
    view.in.camTarget[0] = target.x;
    view.in.camTarget[1] = target.y;
    view.in.camTarget[2] = target.z;
    return view;
}

std::vector<SceneVertex> BasinWithWalker(const Walk& walk, double seconds)
{
    std::vector<SceneVertex> scene = water_checks::BuildBasinScene();
    AddPebbleField(scene);
    const Vec3 feet = FeetAt(walk, seconds);
    AddBox(scene, {feet.x - kUnitHalfWidth, feet.y - kUnitHalfWidth, feet.z},
           {feet.x + kUnitHalfWidth, feet.y + kUnitHalfWidth, kWaterSurfaceZ + kUnitTopAboveSurface}, kUnitColour);
    return scene;
}

WaterFrameResult RenderWalkerFrame(BasinClient& client, const Walk& walk, double seconds,
                                   water_contact_checks::ClientRippleClock* clock)
{
    client.UseScene(BasinWithWalker(walk, seconds));
    WaterContactFrame contacts;
    if (clock)
        contacts.contacts[contacts.count++] = WalkerContact(walk, seconds, *clock);
    water_checks::WaterFrame frame;
    frame.otherPass = false;
    return water_checks::RenderRippleFrame(client, ThirdPersonView(walk, seconds), kStartSeconds + seconds, contacts,
                                           frame);
}

struct WalkCaptures
{
    Image images[kCaptureCount];
    Image normals[kCaptureCount];
    Image calm[kCaptureCount];
    bool shaded = true;
};

WalkCaptures RenderWalk(BasinClient& client, const Walk& walk, const Config& cfg)
{
    Config off = cfg;
    off.waterRipples = 0.0f;
    vf_test_set_config(&off);
    RenderWalkerFrame(client, walk, 0.0, nullptr);
    vf_test_set_config(&cfg);
    water_contact_checks::ClientRippleClock clock;
    WalkCaptures captures;
    int next = 0;
    for (int frame = 0; next < kCaptureCount; ++frame)
    {
        const double seconds = frame * kFrameSeconds;
        const WaterFrameResult result = RenderWalkerFrame(client, walk, seconds, &clock);
        captures.shaded = captures.shaded && result.began;
        if (seconds + kFrameSeconds * 0.5 < kCaptureSeconds[next])
            continue;
        captures.images[next] = result.image;
        Config normals = cfg;
        normals.waterDebugView = kNormalDebugView;
        vf_test_set_config(&normals);
        captures.normals[next] = RenderWalkerFrame(client, walk, seconds, &clock).image;
        Config calm = cfg;
        calm.waterRipples = kUnseenRippleGain;
        vf_test_set_config(&calm);
        captures.calm[next++] = RenderWalkerFrame(client, walk, seconds, &clock).image;
        vf_test_set_config(&cfg);
    }
    return captures;
}

std::wstring CaptureName(const Walk& walk, int capture)
{
    wchar_t name[64];
    std::swprintf(name, 64, L"ripples-%ls-%.1fs", walk.name, kCaptureSeconds[capture]);
    return name;
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
    h.pp.AutoDepthStencilFormat = D3DFMT_D24S8;
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
        const Config cfg = water_checks::WaterConfig(startup);
        const Walk& first = kWalks[0];
        BasinClient client(h, ThirdPersonView(first, 0.0));
        for (const Walk& walk : kWalks)
        {
            const WalkCaptures captures = RenderWalk(client, walk, cfg);
            ok = ok && captures.shaded;
            for (int i = 0; i < kCaptureCount; ++i)
            {
                water_checks::SaveImage(outDir, CaptureName(walk, i).c_str(), captures.images[i]);
                water_checks::SaveImage(outDir, (CaptureName(walk, i) + L"-normals").c_str(), captures.normals[i]);
                water_checks::SaveImage(outDir, (CaptureName(walk, i) + L"-calm").c_str(), captures.calm[i]);
            }
            std::printf("%ls: %s, %d captures\n", walk.name, captures.shaded ? "shaded" : "not shaded",
                        kCaptureCount);
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
