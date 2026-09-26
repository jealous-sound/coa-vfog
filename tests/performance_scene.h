#pragma once

namespace performance_scene
{
constexpr int kWarmupFrames = 32;
constexpr int kMeasuredFrames = 60;
constexpr ULONGLONG kQueryTimeoutMs = 10000;

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

bool MeasureCases(Harness& h)
{
    Timer timer;
    if (!timer.Create(h.dev))
        return false;
    D3DADAPTER_IDENTIFIER9 adapter = {};
    D3DCAPS9 caps = {};
    h.d3d->GetAdapterIdentifier(0, 0, &adapter);
    h.dev->GetDeviceCaps(&caps);
    std::printf("1080p fog benchmark: %s; %s\n", adapter.Description, timer.Method());
    std::printf("pixel shader slots %lu, executed instructions %lu; %d warmup and %d measured frames\n",
                caps.MaxPixelShader30InstructionSlots, caps.MaxPShaderInstructionsExecuted,
                kWarmupFrames, kMeasuredFrames);
    const Vec3 eye = Add({0, -20, 8}, kGameLikeWorldOffset);
    const Vec3 at = Add({200, 0, 8}, kGameLikeWorldOffset);
    float view[16];
    float projection[16];
    CameraRelativeLookAt(eye, at, view);
    EngineGlDepthProjection(1.0f / std::tan(kFovY * 0.5f), 1920.0f / 1080.0f, kNear, kFar, projection);
    const D3DVIEWPORT9 viewport = {0, 0, 1920, 1080, 0, kClientWorldMaxZ};
    const FrameInputs original = MakeInputs(view, projection, eye, at, viewport);
    std::printf("quality,point_lights,median_ms,p95_ms\n");
    for (int quality = 1; quality <= 3; ++quality)
    {
        for (int variant = 0; variant < 2; ++variant)
        {
            Config config;
            config.quality = quality;
            config.worldShadows = false;
            config.logLevel = 0;
            config.overlay = false;
            config.dataMode = 0;
            FrameInputs inputs = original;
            if (variant > 0)
            {
                for (int index = 0; index < 8; ++index)
                {
                    LocalPointLight light = {};
                    light.position[0] = kGameLikeWorldOffset.x + 25.0f + 65.0f * (index / 4);
                    light.position[1] = kGameLikeWorldOffset.y - 45.0f + 30.0f * (index % 4);
                    light.position[2] = kGameLikeWorldOffset.z + 12.0f;
                    light.color[0] = 1.0f;
                    light.color[1] = 0.5f;
                    light.color[2] = 0.2f;
                    light.attenuation[0] = 1.0f;
                    light.attenuation[2] = 0.015f;
                    if (!engine::SelectLocalPointLight(inputs.localLights, light, inputs.camPos))
                        return false;
                }
            }
            std::vector<double> samples;
            if (!Measure(h, timer, config, inputs, eye, samples))
                return false;
            std::sort(samples.begin(), samples.end());
            const double median = (samples[kMeasuredFrames / 2 - 1] + samples[kMeasuredFrames / 2]) * 0.5;
            const double p95 = samples[(kMeasuredFrames * 95 + 99) / 100 - 1];
            std::printf("%d,%u,%.3f,%.3f\n", quality, inputs.localLights.pointLightCount, median, p95);
            std::fflush(stdout);
        }
    }
    return true;
}
}

int RunPerformance()
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
    initial.logLevel = 0;
    vf_test_set_config(&initial);
    if (h.window && create)
        h.d3d = vf_test_wrap_direct3d9(create, D3D_SDK_VERSION);
    h.pp.Windowed = TRUE;
    h.pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    h.pp.BackBufferWidth = 1920;
    h.pp.BackBufferHeight = 1080;
    h.pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    h.pp.EnableAutoDepthStencil = TRUE;
    h.pp.AutoDepthStencilFormat = D3DFMT_D24S8;
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
