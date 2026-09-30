#pragma once

namespace performance_scene
{
constexpr int kWarmupFrames = 32;
constexpr int kMeasuredFrames = 60;
constexpr ULONGLONG kQueryTimeoutMs = 10000;
constexpr ULONGLONG kGpuClockWarmupMs = 1500;
constexpr int kHeaviestQuality = 3;
constexpr Vec3 kEyeAboveStreet = {0, -20, 8};
constexpr Vec3 kTargetAboveStreet = {200, 0, 8};
constexpr int kFloodLights = 8;
constexpr float kFloodLightColor[3] = {1.0f, 0.5f, 0.2f};
constexpr float kFloodLightQuadraticAttenuation = 0.015f;
constexpr float kLampHeight = 5.0f;
constexpr float kLampColor[3] = {1.0f, 0.72f, 0.4f};
constexpr float kLampPeakColor = std::max({kLampColor[0], kLampColor[1], kLampColor[2]});
constexpr int kErrorLogLevel = 0;
constexpr float kFullStorm = 1.0f;
constexpr int kShippedLogLevel = 1;

enum class FogSource
{
    Derived,
    Classic,
    ClassicStorm,
};

enum class PointLights
{
    None,
    Flood,
    Lamps,
};

struct BenchmarkCase
{
    const char* name;
    FogSource fog;
    PointLights lights;
    int logLevel;
};

constexpr BenchmarkCase kCases[] = {
    {"derived-none", FogSource::Derived, PointLights::None, kErrorLogLevel},
    {"derived-flood8", FogSource::Derived, PointLights::Flood, kErrorLogLevel},
    {"derived-lamps8", FogSource::Derived, PointLights::Lamps, kErrorLogLevel},
    {"classic-none", FogSource::Classic, PointLights::None, kErrorLogLevel},
    {"classic-lamps8", FogSource::Classic, PointLights::Lamps, kErrorLogLevel},
    {"classic-storm", FogSource::ClassicStorm, PointLights::None, kErrorLogLevel},
    {"classic-storm-lamps8", FogSource::ClassicStorm, PointLights::Lamps, kErrorLogLevel},
    {"derived-none-log1", FogSource::Derived, PointLights::None, kShippedLogLevel},
};

struct StreetLamp
{
    float ahead;
    float side;
    float reach;
};

constexpr StreetLamp kStreetLamps[] = {
    {30.0f, -9.0f, 12.0f}, {45.0f, 9.0f, 16.0f}, {60.0f, -9.0f, 10.0f}, {75.0f, 9.0f, 20.0f},
    {90.0f, -9.0f, 14.0f}, {105.0f, 9.0f, 18.0f}, {120.0f, -9.0f, 11.0f}, {140.0f, 9.0f, 15.0f},
};

struct Street
{
    Vec3 ground;
    Vec3 eye;
    Vec3 at;
};

class Timer
{
public:
    ~Timer()
    {
        for (auto* query : {m_event, m_start, m_end, m_frequency, m_disjoint})
            if (query)
                query->Release();
    }

    bool Create(IDirect3DDevice9* device)
    {
        QueryPerformanceFrequency(&m_cpuFrequency);
        if (FAILED(device->CreateQuery(D3DQUERYTYPE_EVENT, &m_event)))
            return false;
        m_gpu = SUCCEEDED(device->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &m_start)) &&
                SUCCEEDED(device->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &m_end)) &&
                SUCCEEDED(device->CreateQuery(D3DQUERYTYPE_TIMESTAMPFREQ, &m_frequency)) &&
                SUCCEEDED(device->CreateQuery(D3DQUERYTYPE_TIMESTAMPDISJOINT, &m_disjoint));
        return true;
    }

    bool Begin()
    {
        if (m_gpu)
            return SUCCEEDED(m_disjoint->Issue(D3DISSUE_BEGIN)) && SUCCEEDED(m_start->Issue(D3DISSUE_END));
        if (FAILED(m_event->Issue(D3DISSUE_END)) || !Wait())
            return false;
        QueryPerformanceCounter(&m_cpuStart);
        return true;
    }

    bool End()
    {
        if (m_gpu && (FAILED(m_end->Issue(D3DISSUE_END)) || FAILED(m_frequency->Issue(D3DISSUE_END)) ||
                      FAILED(m_disjoint->Issue(D3DISSUE_END))))
            return false;
        return SUCCEEDED(m_event->Issue(D3DISSUE_END));
    }

    bool Resolve(double& milliseconds)
    {
        if (!Wait())
            return false;
        if (!m_gpu)
        {
            LARGE_INTEGER end;
            QueryPerformanceCounter(&end);
            milliseconds = 1000.0 * (end.QuadPart - m_cpuStart.QuadPart) / m_cpuFrequency.QuadPart;
            return true;
        }
        UINT64 start = 0;
        UINT64 end = 0;
        UINT64 frequency = 0;
        BOOL disjoint = TRUE;
        if (m_start->GetData(&start, sizeof(start), 0) != S_OK ||
            m_end->GetData(&end, sizeof(end), 0) != S_OK ||
            m_frequency->GetData(&frequency, sizeof(frequency), 0) != S_OK ||
            m_disjoint->GetData(&disjoint, sizeof(disjoint), 0) != S_OK || disjoint || !frequency || end < start)
            return false;
        milliseconds = 1000.0 * static_cast<double>(end - start) / static_cast<double>(frequency);
        return true;
    }

    const char* Method() const { return m_gpu ? "GPU timestamps" : "event-flushed CPU/GPU elapsed time"; }

private:
    bool Wait()
    {
        const ULONGLONG started = GetTickCount64();
        for (;;)
        {
            const HRESULT result = m_event->GetData(nullptr, 0, D3DGETDATA_FLUSH);
            if (result != S_FALSE)
                return result == S_OK;
            if (GetTickCount64() - started >= kQueryTimeoutMs)
                return false;
            Sleep(0);
        }
    }

    IDirect3DQuery9* m_event = nullptr;
    IDirect3DQuery9* m_start = nullptr;
    IDirect3DQuery9* m_end = nullptr;
    IDirect3DQuery9* m_frequency = nullptr;
    IDirect3DQuery9* m_disjoint = nullptr;
    LARGE_INTEGER m_cpuFrequency = {};
    LARGE_INTEGER m_cpuStart = {};
    bool m_gpu = false;
};

Vec3 Scaled(Vec3 v, float s)
{
    return {v.x * s, v.y * s, v.z * s};
}

Street StreetOn(Vec3 ground)
{
    return {ground, Add(kEyeAboveStreet, ground), Add(kTargetAboveStreet, ground)};
}

std::vector<SceneVertex> SceneAlong(const Street& street)
{
    std::vector<SceneVertex> scene = BuildScene();
    const Vec3 shift = Sub(street.ground, kGameLikeWorldOffset);
    for (SceneVertex& vertex : scene)
    {
        vertex.x += shift.x;
        vertex.y += shift.y;
        vertex.z += shift.z;
    }
    return scene;
}

void UseHarbourSunset(FrameInputs& inputs)
{
    const FrameInputs harbour =
        ContinentFrame(kEasternKingdoms, kHarbourEye, kHarbourDayFraction, kHarbourToLight, false);
    inputs.mapId = harbour.mapId;
    inputs.dayFraction = harbour.dayFraction;
    std::memcpy(inputs.toLight, harbour.toLight, sizeof(inputs.toLight));
    inputs.lightIsMoon = harbour.lightIsMoon;
    inputs.fogStart = harbour.fogStart;
    inputs.fogEnd = harbour.fogEnd;
    inputs.zoneFogDistance = harbour.zoneFogDistance;
}

FrameInputs StreetInputs(const Street& street, FogSource fog, const float* projection, const D3DVIEWPORT9& viewport)
{
    float view[16];
    CameraRelativeLookAt(street.eye, street.at, view);
    FrameInputs inputs = MakeInputs(view, projection, street.eye, street.at, viewport);
    if (fog != FogSource::Derived)
        UseHarbourSunset(inputs);
    if (fog == FogSource::ClassicStorm)
        inputs.lightParams.stormBlend = kFullStorm;
    return inputs;
}

bool AddPointLight(FrameInputs& inputs, Vec3 position, const float* color, float quadraticAttenuation)
{
    LocalPointLight light = {};
    light.position[0] = position.x;
    light.position[1] = position.y;
    light.position[2] = position.z;
    std::memcpy(light.color, color, sizeof(light.color));
    light.attenuation[0] = 1.0f;
    light.attenuation[2] = quadraticAttenuation;
    return engine::SelectLocalPointLight(inputs.localLights, light, inputs.camPos);
}

bool AddFloodLights(FrameInputs& inputs, const Street& street)
{
    for (int index = 0; index < kFloodLights; ++index)
    {
        const Vec3 position = {street.ground.x + 25.0f + 65.0f * (index / 4),
                               street.ground.y - 45.0f + 30.0f * (index % 4), street.ground.z + 12.0f};
        if (!AddPointLight(inputs, position, kFloodLightColor, kFloodLightQuadraticAttenuation))
            return false;
    }
    return true;
}

float QuadraticAttenuationReaching(float reach)
{
    return (kLampPeakColor / kLocalPointLightContributionCutoff - 1.0f) / (reach * reach);
}

bool AddStreetLamps(FrameInputs& inputs, const Street& street)
{
    const Vec3 forward = Norm(Sub(street.at, street.eye));
    const Vec3 right = Norm(Cross(forward, kWorldUp));
    for (const StreetLamp& lamp : kStreetLamps)
    {
        const Vec3 alongStreet = Add(street.eye, Add(Scaled(forward, lamp.ahead), Scaled(right, lamp.side)));
        const Vec3 position = {alongStreet.x, alongStreet.y, street.ground.z + kLampHeight};
        if (!AddPointLight(inputs, position, kLampColor, QuadraticAttenuationReaching(lamp.reach)))
            return false;
    }
    return true;
}

bool AddPointLights(FrameInputs& inputs, PointLights lights, const Street& street)
{
    if (lights == PointLights::Flood)
        return AddFloodLights(inputs, street);
    if (lights == PointLights::Lamps)
        return AddStreetLamps(inputs, street);
    return true;
}

void DescribeLights(const char* name, const FrameInputs& inputs)
{
    const LocalLightInputs& lights = inputs.localLights;
    float nearest = std::numeric_limits<float>::infinity();
    float farthest = 0.0f;
    float shortestReach = kMaxLocalPointLightRadius;
    float longestReach = 0.0f;
    for (uint32_t i = 0; i < lights.pointLightCount; ++i)
    {
        const LocalPointLight& light = lights.pointLights[i];
        const Vec3 offset = Sub({light.position[0], light.position[1], light.position[2]},
                                {inputs.camPos[0], inputs.camPos[1], inputs.camPos[2]});
        const float distance = std::sqrt(Dot(offset, offset));
        nearest = std::fmin(nearest, distance);
        farthest = std::fmax(farthest, distance);
        shortestReach = std::fmin(shortestReach, light.cutoff);
        longestReach = std::fmax(longestReach, light.cutoff);
    }
    std::printf("%s: %u point lights reaching %.1f-%.1f yd, %.0f-%.0f yd from the camera\n", name,
                lights.pointLightCount, shortestReach, longestReach, nearest, farthest);
}

std::string FogDataBesideTheFogDll()
{
    char path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(GetModuleHandleA("CoAVolFog.dll"), path, MAX_PATH);
    const std::string module(path, length);
    return module.substr(0, module.find_last_of("\\/") + 1) + "fogdata.bin";
}

bool ClassicFogResolves(const FrameInputs& inputs, int& layers, int& noisyLayers)
{
    FogData data;
    AuthoredFog fog = {};
    const bool resolved = data.Load(FogDataBesideTheFogDll()) &&
                          data.Resolve(inputs.mapId, inputs.camPos, inputs.dayFraction, inputs.lightParams, fog);
    layers = fog.layerCount;
    noisyLayers = 0;
    for (int i = 0; i < fog.layerCount; ++i)
        noisyLayers += fog.layers[i].noise.presence > 0.0f ? 1 : 0;
    return resolved;
}

bool Measure(Harness& h, Timer& timer, const Config& config, const FrameInputs& inputs, Vec3 eye,
             std::vector<double>& samples)
{
    vf_test_set_config(&config);
    for (int frame = 0; frame < kWarmupFrames + kMeasuredFrames; ++frame)
    {
        h.BeginFrame();
        h.DrawScene(eye, inputs.cameraRelativeView, inputs.glProjection, inputs.viewport);
        const bool started = timer.Begin();
        const char* reason = "";
        const bool rendered = started && vf_test_render(&inputs, &reason);
        const bool ended = timer.End();
        h.dev->EndScene();
        double milliseconds = 0;
        if (!rendered || !ended || !timer.Resolve(milliseconds))
        {
            std::printf("performance measurement failed: %s\n", reason);
            return false;
        }
        if (frame >= kWarmupFrames)
            samples.push_back(milliseconds);
        if (FAILED(h.dev->Present(nullptr, nullptr, nullptr, nullptr)))
            return false;
    }
    return true;
}

bool WarmUpGpuClocks(Harness& h, Timer& timer, const Street& street, const float* projection,
                     const D3DVIEWPORT9& viewport)
{
    Config config;
    config.quality = kHeaviestQuality;
    config.logLevel = kErrorLogLevel;
    config.overlay = false;
    config.dataMode = 0;
    FrameInputs inputs = StreetInputs(street, FogSource::Derived, projection, viewport);
    if (!AddFloodLights(inputs, street))
        return false;
    h.scene = SceneAlong(street);
    const ULONGLONG started = GetTickCount64();
    int frames = 0;
    do
    {
        std::vector<double> untimed;
        if (!Measure(h, timer, config, inputs, street.eye, untimed))
            return false;
        frames += kWarmupFrames + kMeasuredFrames;
    } while (GetTickCount64() - started < kGpuClockWarmupMs);
    std::printf("GPU clock warm-up: %d untimed frames of quality %d with flood8\n", frames, kHeaviestQuality);
    return true;
}

bool MeasureCases(Harness& h)
{
    Timer timer;
    if (!timer.Create(h.dev))
        return false;
    D3DADAPTER_IDENTIFIER9 adapter = {};
    D3DCAPS9 caps = {};
    h.d3d->GetAdapterIdentifier(0, 0, &adapter);
    h.dev->GetDeviceCaps(&caps);
    MultisamplingStatus multisampling;
    vf_test_multisampling(&multisampling);
    std::printf("1080p fog benchmark: %s; %s\n", adapter.Description, timer.Method());
    if (multisampling.method && *multisampling.method)
        std::printf("multisampling %dx kept, depth copied by %s\n", multisampling.samples, multisampling.method);
    else
        std::printf("multisampling off: %s\n", multisampling.off);
    std::printf("pixel shader slots %lu, executed instructions %lu; %d warmup and %d measured frames\n",
                caps.MaxPixelShader30InstructionSlots, caps.MaxPShaderInstructionsExecuted,
                kWarmupFrames, kMeasuredFrames);
    float projection[16];
    EngineGlDepthProjection(1.0f / std::tan(kFovY * 0.5f), 1920.0f / 1080.0f, kNear, kFar, projection);
    const D3DVIEWPORT9 viewport = {0, 0, 1920, 1080, 0, kClientWorldMaxZ};
    const Street derivedStreet = StreetOn(kGameLikeWorldOffset);
    const Street harbourStreet = StreetOn(Sub(kHarbourEye, kEyeAboveStreet));

    FrameInputs described = StreetInputs(derivedStreet, FogSource::Derived, projection, viewport);
    if (!AddFloodLights(described, derivedStreet))
        return false;
    DescribeLights("flood8", described);
    described = StreetInputs(derivedStreet, FogSource::Derived, projection, viewport);
    if (!AddStreetLamps(described, derivedStreet))
        return false;
    DescribeLights("lamps8", described);
    int classicLayers = 0;
    int noisyLayers = 0;
    const bool classic = ClassicFogResolves(StreetInputs(harbourStreet, FogSource::Classic, projection, viewport),
                                            classicLayers, noisyLayers);
    int stormLayers = 0;
    int noisyStormLayers = 0;
    ClassicFogResolves(StreetInputs(harbourStreet, FogSource::ClassicStorm, projection, viewport), stormLayers,
                       noisyStormLayers);
    if (classic)
        std::printf("classic: the street moved to the Stormwind harbour at 18:43, %d Classic fog layers (%d with "
                    "authored noise); in a full storm %d layers (%d with authored noise)\n",
                    classicLayers, noisyLayers, stormLayers, noisyStormLayers);
    else
        std::printf("classic cases skipped: fogdata.bin beside CoAVolFog.dll does not resolve the harbour\n");

    if (!WarmUpGpuClocks(h, timer, derivedStreet, projection, viewport))
        return false;
    std::printf("quality,case,point_lights,median_ms,p95_ms\n");
    for (int quality = 1; quality <= 3; ++quality)
    {
        for (const BenchmarkCase& benchmark : kCases)
        {
            const bool classicCase = benchmark.fog != FogSource::Derived;
            if (classicCase && !classic)
                continue;
            const Street& street = classicCase ? harbourStreet : derivedStreet;
            Config config;
            config.quality = quality;
            config.logLevel = benchmark.logLevel;
            config.overlay = false;
            config.dataMode = classicCase ? 1 : 0;
            FrameInputs inputs = StreetInputs(street, benchmark.fog, projection, viewport);
            if (!AddPointLights(inputs, benchmark.lights, street))
                return false;
            h.scene = SceneAlong(street);
            std::vector<double> samples;
            if (!Measure(h, timer, config, inputs, street.eye, samples))
                return false;
            std::sort(samples.begin(), samples.end());
            const double median = (samples[kMeasuredFrames / 2 - 1] + samples[kMeasuredFrames / 2]) * 0.5;
            const double p95 = samples[(kMeasuredFrames * 95 + 99) / 100 - 1];
            std::printf("%d,%s,%u,%.3f,%.3f\n", quality, benchmark.name, inputs.localLights.pointLightCount, median,
                        p95);
            std::fflush(stdout);
        }
    }
    return true;
}
}

int RunPerformance(D3DMULTISAMPLE_TYPE samples)
{
    WNDCLASSW windowClass = {};
    windowClass.lpfnWndProc = DefWindowProcW;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = L"vfog_performance";
    RegisterClassW(&windowClass);
    Harness h;
    h.window = CreateWindowW(windowClass.lpszClassName, L"vfog performance", WS_OVERLAPPEDWINDOW,
                             0, 0, 1920, 1080, nullptr, nullptr, windowClass.hInstance, nullptr);
    HMODULE module = LoadLibraryA("d3d9.dll");
    const auto create = module ?
        reinterpret_cast<IDirect3D9*(WINAPI*)(UINT)>(GetProcAddress(module, "Direct3DCreate9")) : nullptr;
    Config initial;
    initial.overlay = false;
    initial.logLevel = performance_scene::kErrorLogLevel;
    vf_test_set_config(&initial);
    if (h.window && create)
        h.d3d = vf_test_wrap_direct3d9(create, D3D_SDK_VERSION);
    h.pp.Windowed = TRUE;
    h.pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    h.pp.BackBufferWidth = 1920;
    h.pp.BackBufferHeight = 1080;
    h.pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    h.pp.EnableAutoDepthStencil = TRUE;
    h.pp.AutoDepthStencilFormat = kClientDepthFormat;
    h.pp.MultiSampleType = samples;
    h.pp.hDeviceWindow = h.window;
    h.pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    const DWORD flags = D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE | D3DCREATE_FPU_PRESERVE;
    bool completed = false;
    if (h.d3d && SUCCEEDED(h.d3d->CreateDevice(0, D3DDEVTYPE_HAL, h.window, flags, &h.pp, &h.dev)) && h.dev)
        completed = performance_scene::MeasureCases(h);
    if (h.dev)
        h.dev->Release();
    if (h.d3d)
        h.d3d->Release();
    if (module)
        FreeLibrary(module);
    if (h.window)
        DestroyWindow(h.window);
    UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
    return completed ? 0 : 1;
}
