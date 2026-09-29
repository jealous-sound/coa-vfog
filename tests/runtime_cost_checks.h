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
