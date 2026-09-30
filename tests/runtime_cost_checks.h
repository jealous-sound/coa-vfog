#pragma once

namespace runtime_cost
{
constexpr ULONGLONG kQueryTimeoutMs = 10000;
constexpr unsigned kFramesBeforeFirstDepthProbe = 60;
constexpr int kMaxFramesUntilProbeLogged = 240;
constexpr int kDepthProbeRows = 5;
constexpr UINT kProbeDeviceWidth = 640;
constexpr UINT kProbeDeviceHeight = 360;
constexpr DWORD kShrinkViewportForSummary = 40;
constexpr UINT kHistoryPassesSkipped = 2;

bool WaitForQuery(IDirect3DQuery9* query, void* data, DWORD size)
{
    const ULONGLONG started = GetTickCount64();
    for (;;)
    {
        const HRESULT result = query->GetData(data, size, D3DGETDATA_FLUSH);
        if (result != S_FALSE)
            return result == S_OK;
        if (GetTickCount64() - started >= kQueryTimeoutMs)
            return false;
        Sleep(0);
    }
}

bool CountFogPixels(Harness& h, const FrameInputs& in, const float* view, const float* proj, Vec3 eye,
                    IDirect3DQuery9* pixels, DWORD& drawn)
{
    h.BeginFrame();
    h.DrawScene(eye, view, proj, in.viewport);
    const char* skip = "";
    const bool rendered = SUCCEEDED(pixels->Issue(D3DISSUE_BEGIN)) && vf_test_render(&in, &skip) != 0 &&
                          SUCCEEDED(pixels->Issue(D3DISSUE_END));
    h.dev->EndScene();
    h.dev->Present(nullptr, nullptr, nullptr, nullptr);
    return rendered && WaitForQuery(pixels, &drawn, sizeof(drawn));
}

void CheckDisabledTemporalSkipsHistoryPasses(Harness& h, Vec3 eye, Vec3 at, const float* proj,
                                             const D3DVIEWPORT9& world)
{
    Config saved;
    vf_test_get_config(&saved);
    Config config = saved;
    config.godRays = 0.0f;
    config.logLevel = 0;
    float view[16];
    CameraRelativeLookAt(eye, at, view);
    const FrameInputs in = MakeInputs(view, proj, eye, at, world);
    IDirect3DQuery9* pixels = nullptr;
    bool counted = SUCCEEDED(h.dev->CreateQuery(D3DQUERYTYPE_OCCLUSION, &pixels));
    DWORD drawn[2] = {};
    const float weights[2] = {0.85f, 0.0f};
    for (int i = 0; i < 2 && counted; ++i)
    {
        config.temporal = weights[i];
        vf_test_set_config(&config);
        counted = CountFogPixels(h, in, view, proj, eye, pixels, drawn[i]);
    }
    if (pixels)
        pixels->Release();
    const UINT scale = config.quality == 1 ? 4u : 2u;
    const DWORD lowResolutionPass = ((world.Width + scale - 1) / scale) * ((world.Height + scale - 1) / scale);
    std::printf("     fog pixels drawn: %lu with temporal filtering, %lu without (a low-resolution pass is %lu)\n",
                drawn[0], drawn[1], lowResolutionPass);
    Check(counted && drawn[1] > 0 && drawn[0] - drawn[1] == kHistoryPassesSkipped * lowResolutionPass,
          "with Temporal=0 the fog composites the march result and skips the temporal and history-depth passes");
    vf_test_set_config(&saved);
}

std::wstring FogLogBesideTheFogDll()
{
    wchar_t path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(GetModuleHandleW(L"CoAVolFog.dll"), path, MAX_PATH);
    const std::wstring module(path, length);
    return module.substr(0, module.find_last_of(L"\\/") + 1) + L"CoAVolFog.log";
}

std::string LogWrittenSince(size_t offset)
{
    const std::string text = ReadText(FogLogBesideTheFogDll());
    return text.size() > offset ? text.substr(offset) : std::string();
}

bool HasLine(const std::string& text, const char* fragment)
{
    return text.find(fragment) != std::string::npos;
}

bool DepthProbeLogged(const std::string& text)
{
    if (!HasLine(text, "depth probe 1: day "))
        return false;
    for (int row = 0; row < kDepthProbeRows; ++row)
    {
        char label[32];
        std::snprintf(label, sizeof(label), "  row %d:  ", row);
        if (!HasLine(text, label))
            return false;
    }
    return true;
}

bool LastFogGpuTime(const std::string& text, float& medianMs, unsigned& frames, unsigned& skipped)
{
    const size_t at = text.rfind(", fog gpu ");
    return at != std::string::npos &&
           std::sscanf(text.c_str() + at, ", fog gpu %f ms (median of %u frames, %u skipped)", &medianMs, &frames,
                       &skipped) == 3;
}

void CheckDepthProbeAndGpuTimeLog(Harness& h, Vec3 eye, Vec3 at)
{
    Config saved;
    vf_test_get_config(&saved);
    Config config;
    config.overlay = false;
    config.logLevel = 1;
    config.godRays = 0.0f;
    vf_test_set_config(&config);

    Harness probe;
    probe.d3d = h.d3d;
    probe.window = CreateWindowW(L"vfog_harness", L"vfog probe", WS_OVERLAPPEDWINDOW, 0, 0, kProbeDeviceWidth,
                                 kProbeDeviceHeight, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    probe.pp = h.pp;
    probe.pp.BackBufferWidth = kProbeDeviceWidth;
    probe.pp.BackBufferHeight = kProbeDeviceHeight;
    probe.pp.hDeviceWindow = probe.window;
    const DWORD engineFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE | D3DCREATE_FPU_PRESERVE;
    const bool created = probe.window && SUCCEEDED(h.d3d->CreateDevice(0, D3DDEVTYPE_HAL, probe.window, engineFlags,
                                                                       &probe.pp, &probe.dev)) && probe.dev;
    Check(created, "a second fog device starts with a fresh renderer for the log checks");
    if (!created)
    {
        if (probe.window)
            DestroyWindow(probe.window);
        vf_test_set_config(&saved);
        return;
    }

    const D3DVIEWPORT9 world = {0, 0, kProbeDeviceWidth, kProbeDeviceHeight, 0.0f, 1.0f};
    float proj[16];
    float view[16];
    EngineProjection(static_cast<float>(kProbeDeviceWidth) / kProbeDeviceHeight, proj);
    CameraRelativeLookAt(eye, at, view);
    const FrameInputs in = MakeInputs(view, proj, eye, at, world);
    const size_t logStart = ReadText(FogLogBesideTheFogDll()).size();
    bool rendered = true;
    auto frame = [&](const FrameInputs& input) {
        probe.BeginFrame();
        probe.DrawScene(eye, view, proj, input.viewport);
        const char* skip = "";
        rendered = vf_test_render(&input, &skip) != 0 && rendered;
        probe.dev->EndScene();
        probe.dev->Present(nullptr, nullptr, nullptr, nullptr);
    };
    for (unsigned i = 0; i <= kFramesBeforeFirstDepthProbe; ++i)
        frame(in);
    const bool deferred = !HasLine(LogWrittenSince(logStart), "depth probe 1:");
    int laterFrames = 0;
    while (!DepthProbeLogged(LogWrittenSince(logStart)) && laterFrames < kMaxFramesUntilProbeLogged)
    {
        frame(in);
        ++laterFrames;
    }
    const bool logged = DepthProbeLogged(LogWrittenSince(logStart));
    std::printf("     depth probe issued on frame %u, logged %d frame(s) later\n", kFramesBeforeFirstDepthProbe,
                laterFrames);
    Check(rendered && deferred && logged,
          "the depth probe reads back on a later frame instead of waiting for the GPU, and still logs all rows");

    FrameInputs shrunk = in;
    shrunk.viewport.Height -= kShrinkViewportForSummary;
    frame(shrunk);
    float medianMs = 0.0f;
    unsigned frames = 0;
    unsigned skipped = 0;
    const bool timed = LastFogGpuTime(LogWrittenSince(logStart), medianMs, frames, skipped);
    std::printf("     frame summary: fog gpu %.3f ms over %u frames, %u skipped\n", medianMs, frames, skipped);
    Check(rendered && timed && frames > 0 && medianMs > 0.0f,
          "the frame summary reports the fog's GPU time from timestamp queries");

    probe.dev->Release();
    DestroyWindow(probe.window);
    vf_test_set_config(&saved);
}

std::string StormSummaryAtHarbour(Harness& h, Config config, bool classicNoise, const D3DVIEWPORT9& world)
{
    config.classicNoise = classicNoise;
    vf_test_set_config(&config);
    const Vec3 at = Add(kHarbourEye, {100, 100, -5});
    float view[16];
    float projection[16];
    CameraRelativeLookAt(kHarbourEye, at, view);
    EngineProjection(static_cast<float>(world.Width) / world.Height, projection);
    FrameInputs input = MakeInputs(view, projection, kHarbourEye, at, world);
    input.mapId = kEasternKingdoms;
    input.dayFraction = kNoon;
    input.lightParams = Storm(1.0f);
    const size_t logStart = ReadText(FogLogBesideTheFogDll()).size();
    h.BeginFrame();
    h.DrawScene(kHarbourEye, view, projection, world);
    const char* skip = "";
    const bool rendered = vf_test_render(&input, &skip) != 0;
    h.dev->EndScene();
    h.dev->Present(nullptr, nullptr, nullptr, nullptr);
    return rendered ? LogWrittenSince(logStart) : std::string();
}

void CheckFrameSummaryLogsClassicExtras(Harness& h)
{
    Config saved;
    vf_test_get_config(&saved);
    Config config = saved;
    config.logLevel = 1;
    config.dataMode = 1;
    config.godRays = 0.0f;
    config.temporal = 0.0f;
    config.localLights = false;
    const D3DVIEWPORT9 noisyView = {0, 0, 126, 94, 0, 1};
    const D3DVIEWPORT9 quietView = {0, 0, 122, 92, 0, 1};
    const std::string noisy = StormSummaryAtHarbour(h, config, true, noisyView);
    const std::string quiet = StormSummaryAtHarbour(h, config, false, quietView);
    const size_t noiseLine = noisy.find("  classic layer ");
    std::printf("     storm summary noise line: %s\n",
                noiseLine == std::string::npos ? "missing" : noisy.substr(noiseLine, noisy.find('\n', noiseLine) -
                                                                                        noiseLine).c_str());
    Check(HasLine(noisy, "  Classic glow ") && HasLine(noisy, "(not rendered)") &&
              HasLine(noisy, " noise: share 1.00, drawn alpha 1.00;"),
          "the frame summary logs the Classic glow, the grading curve and each noisy layer's drawn noise");
    Check(HasLine(quiet, " noise: share 1.00, drawn alpha 0.00 (off: ClassicNoise=0);"),
          "with ClassicNoise=0 the frame summary logs the authored noise as off, not as drawn");
    vf_test_set_config(&saved);
}

constexpr int kFramesPastLightSetSettling = 40;
constexpr float kFarClipStepThatLogsASummary = 100.0f;
constexpr float kLoggedLightAttenuation[3] = {0.0f, 0.7f, 0.03f};

size_t CountOf(const std::string& text, const char* fragment)
{
    size_t count = 0;
    for (size_t at = text.find(fragment); at != std::string::npos; at = text.find(fragment, at + 1))
        ++count;
    return count;
}

LocalPointLight LoggedLight(Vec3 eye, Vec3 forward, float yardsAhead, const float (&colour)[3], uintptr_t id)
{
    LocalPointLight light;
    const Vec3 position = Add(eye, {forward.x * yardsAhead, forward.y * yardsAhead, forward.z * yardsAhead});
    light.position[0] = position.x;
    light.position[1] = position.y;
    light.position[2] = position.z;
    std::memcpy(light.color, colour, sizeof(light.color));
    std::memcpy(light.attenuation, kLoggedLightAttenuation, sizeof(light.attenuation));
    light.nativeId = id;
    return light;
}

void CheckLocalLightLogLines(Harness& h)
{
    Config saved;
    vf_test_get_config(&saved);
    Config config = saved;
    config.logLevel = 1;
    config.quality = 1;
    config.dataMode = 0;
    config.temporal = 0.0f;
    config.godRays = 0.0f;
    config.localLights = true;
    vf_test_set_config(&config);
    const D3DVIEWPORT9 world = {0, 0, 128, 96, 0, 1};
    const Vec3 eye = Add(kGameLikeWorldOffset, {0, 0, 9});
    const Vec3 at = Add(kGameLikeWorldOffset, {100, 0, 9});
    const Vec3 forward = Norm(Sub(at, eye));
    float view[16];
    float projection[16];
    CameraRelativeLookAt(eye, at, view);
    EngineProjection(128.0f / 96.0f, projection);
    const FrameInputs empty = MakeInputs(view, projection, eye, at, world);
    const float statue[3] = {65.9f, 255.0f, 255.0f};
    const float lantern[3] = {1.0f, 0.72f, 0.4f};
    const LocalPointLight lights[] = {LoggedLight(eye, forward, 20.0f, statue, 0x1001),
                                      LoggedLight(eye, forward, 40.0f, lantern, 0x1002),
                                      LoggedLight(eye, forward, 60.0f, lantern, 0x1003)};
    auto inputsWith = [&](int count) {
        FrameInputs inputs = empty;
        for (int i = 0; i < count; ++i)
            engine::SelectLocalPointLight(inputs.localLights, lights[i], inputs.camPos, LocalLightUpload(config));
        return inputs;
    };
    const FrameInputs two = inputsWith(2);
    const FrameInputs three = inputsWith(3);
    bool rendered = two.localLights.pointLightCount == 2 && three.localLights.pointLightCount == 3;
    auto frame = [&](const FrameInputs& inputs) {
        h.BeginFrame();
        h.DrawScene(eye, view, projection, world);
        const char* skip = "";
        rendered = vf_test_render(&inputs, &skip) != 0 && rendered;
        h.dev->EndScene();
        h.dev->Present(nullptr, nullptr, nullptr, nullptr);
    };
    const size_t logStart = ReadText(FogLogBesideTheFogDll()).size();
    frame(two);
    const bool settling = !HasLine(LogWrittenSince(logStart), "local lights:");
    for (int i = 0; i < kFramesPastLightSetSettling; ++i)
        frame(two);
    const std::string infoLog = LogWrittenSince(logStart);
    Check(rendered && settling &&
              CountOf(infoLog, "local lights: 2 uploaded, brightest linear (12.99 255 255), nearest 20.0 yd") == 1 &&
              !HasLine(infoLog, "  local light 0:"),
          "at LogLevel 1 a settled change of the uploaded point lights logs one line with the count, the brightest "
          "uploaded colour and the nearest distance");

    auto logLevel = [&](int level) {
        config.logLevel = level;
        vf_test_set_config(&config);
    };
    logLevel(2);
    const size_t raisedStart = ReadText(FogLogBesideTheFogDll()).size();
    frame(two);
    const std::string raisedLog = LogWrittenSince(raisedStart);
    logLevel(1);
    frame(two);
    logLevel(2);
    const size_t raisedAgainStart = ReadText(FogLogBesideTheFogDll()).size();
    frame(two);
    const std::string raisedAgainLog = LogWrittenSince(raisedAgainStart);
    logLevel(1);
    Check(rendered && CountOf(raisedLog, "local lights: 2 uploaded") == 1 &&
              CountOf(raisedLog, "  local light 0: ") == 1 && CountOf(raisedLog, "  local light 1: ") == 1 &&
              !HasLine(raisedLog, "  local light 2: ") && !HasLine(raisedAgainLog, "local light"),
          "raising LogLevel from 1 to 2 over the same uploaded point lights logs their per-light lines once");

    const size_t flickerStart = ReadText(FogLogBesideTheFogDll()).size();
    for (int i = 0; i < kFramesPastLightSetSettling; ++i)
        frame(i % 2 ? two : three);
    Check(rendered && !HasLine(LogWrittenSince(flickerStart), "local lights:"),
          "a point-light set that changes every frame is not logged until it settles");

    config.logLevel = 2;
    vf_test_set_config(&config);
    const size_t debugStart = ReadText(FogLogBesideTheFogDll()).size();
    for (int i = 0; i < kFramesPastLightSetSettling; ++i)
        frame(three);
    const std::string debugLog = LogWrittenSince(debugStart);
    const size_t firstLight = debugLog.find("  local light 0: ");
    std::printf("     %s\n", firstLight == std::string::npos
                                   ? "no per-light line"
                                   : debugLog.substr(firstLight, debugLog.find('\n', firstLight) - firstLight).c_str());
    Check(rendered && CountOf(debugLog, "local lights: 3 uploaded") == 1 &&
              CountOf(debugLog, "attenuation 0 0.7 0.03, enabled 1; uploaded (") == 3 &&
              HasLine(debugLog, "  local light 0: at (") && HasLine(debugLog, "20.0 yd; diffuse (65.9 255 255)") &&
              HasLine(debugLog, "uploaded (12.99 255 255), reach ") && HasLine(debugLog, "  local light 2: "),
          "at LogLevel 2 each uploaded point light logs its position, distance, captured diffuse, attenuation, "
          "enabled flag, uploaded colour and reach");

    const size_t emptyStart = ReadText(FogLogBesideTheFogDll()).size();
    for (int i = 0; i < kFramesPastLightSetSettling; ++i)
        frame(empty);
    Check(rendered && CountOf(LogWrittenSince(emptyStart), "local lights: none uploaded") == 1,
          "the log says when the last point light stops being uploaded");

    FrameInputs rejected = empty;
    rejected.localLights.capture = LocalLightCapture::DisabledLight;
    rejected.farClip = empty.farClip + kFarClipStepThatLogsASummary;
    const size_t rejectedStart = ReadText(FogLogBesideTheFogDll()).size();
    frame(rejected);
    const std::string firstRejectedFrame = LogWrittenSince(rejectedStart);
    for (int i = 0; i < kFramesPastLightSetSettling; ++i)
        frame(rejected);
    const std::string rejectedLog = LogWrittenSince(rejectedStart);
    Check(rendered &&
              HasLine(firstRejectedFrame,
                      "local lights: capture rejected (disabled light in the table), first on frame ") &&
              HasLine(firstRejectedFrame,
                      "  local points 0 enabled 1, capture rejected (disabled light in the table); interior ") &&
              CountOf(rejectedLog, "capture rejected (disabled light in the table), first on frame ") == 1 &&
              CountOf(rejectedLog, "local lights: none uploaded, capture rejected (disabled light in the table)") ==
                  1,
          "a rejected point-light capture is logged with its reason on its first frame, in the frame summary and "
          "once settled, not as an area without lights");
    vf_test_set_config(&saved);
}

struct ScriptedQueryCreation
{
    HRESULT failure = S_OK;
    unsigned calls = 0;

    QueryCreator Creator()
    {
        return [this](IDirect3DDevice9* dev, D3DQUERYTYPE type, IDirect3DQuery9** query) {
            ++calls;
            return FAILED(failure) ? failure : dev->CreateQuery(type, query);
        };
    }
};

void TimeEmptyFrame(GpuTimer& timer, IDirect3DDevice9* dev)
{
    timer.Begin(dev);
    timer.End();
}

void CheckGpuTimerRetriesTransientCreationFailures(IDirect3DDevice9* dev)
{
    ScriptedQueryCreation afterReset;
    afterReset.failure = D3DERR_OUTOFVIDEOMEMORY;
    GpuTimer resetTimer("fog", afterReset.Creator());
    TimeEmptyFrame(resetTimer, dev);
    const unsigned failedCalls = afterReset.calls;
    TimeEmptyFrame(resetTimer, dev);
    const bool waited = afterReset.calls == failedCalls;
    resetTimer.Release();
    afterReset.failure = S_OK;
    const bool recreatedAfterReset = !resetTimer.Unsupported() && resetTimer.Prepare(dev);
    Check(waited && recreatedAfterReset,
          "fog gpu timing treats D3DERR_OUTOFVIDEOMEMORY as transient and recreates its queries after the next Reset");

    ScriptedQueryCreation later;
    later.failure = E_OUTOFMEMORY;
    GpuTimer laterTimer("fog", later.Creator());
    TimeEmptyFrame(laterTimer, dev);
    later.failure = S_OK;
    unsigned frames = 0;
    do
    {
        TimeEmptyFrame(laterTimer, dev);
        ++frames;
    } while (!laterTimer.Prepare(dev) && frames < 2 * GpuTimer::kFramesBetweenCreationAttempts);
    const GpuTime interval = laterTimer.TakeInterval();
    std::printf("     fog gpu timing queries recreated %u frames after E_OUTOFMEMORY, %u skipped\n", frames,
                interval.skipped);
    Check(!laterTimer.Unsupported() && frames == GpuTimer::kFramesBetweenCreationAttempts &&
              interval.skipped == frames,
          "fog gpu timing retries query creation 600 frames after E_OUTOFMEMORY and counts every frame without "
          "queries as skipped");
}

void CheckGpuTimerReportsUnsupportedBeforeFirstSummary(IDirect3DDevice9* dev)
{
    ScriptedQueryCreation unsupported;
    unsupported.failure = D3DERR_NOTAVAILABLE;
    GpuTimer timer("fog", unsupported.Creator());
    char firstSummary[96] = {};
    DescribeGpuTime(timer, dev, firstSummary, sizeof(firstSummary));
    const unsigned probedCalls = unsupported.calls;
    timer.Release();
    unsupported.failure = S_OK;
    TimeEmptyFrame(timer, dev);
    Check(probedCalls > 0 && firstSummary[0] == 0 && timer.Unsupported() && unsupported.calls == probedCalls,
          "without timestamp queries the first frame summary already omits the fog gpu time, and a Reset does not "
          "retry them");

    ScriptedQueryCreation supported;
    GpuTimer fresh("fog", supported.Creator());
    char freshSummary[96] = {};
    DescribeGpuTime(fresh, dev, freshSummary, sizeof(freshSummary));
    Check(std::strcmp(freshSummary, "fog gpu no samples (0 skipped)") == 0 && !fresh.Unsupported(),
          "with timestamp queries the first frame summary reports that no fog gpu sample has finished yet");
}
}
