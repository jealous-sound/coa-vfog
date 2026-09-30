#pragma once

namespace multisampling_checks
{
using Direct3DCreate = IDirect3D9*(WINAPI*)(UINT);

constexpr D3DMULTISAMPLE_TYPE kFourSamples = D3DMULTISAMPLE_4_SAMPLES;
constexpr DWORD kClientDeviceFlags =
    D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE | D3DCREATE_FPU_PRESERVE;
constexpr DWORD kClientClearFlags = D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER;
constexpr D3DFORMAT kStencilDepthFormat = D3DFMT_D24S8;
constexpr unsigned kNvidiaVendorId = 0x10DE;
constexpr D3DFORMAT kResz = static_cast<D3DFORMAT>(MAKEFOURCC('R', 'E', 'S', 'Z'));
constexpr int kDepthCopyFromDriver = kDepthCopyMethodFromDriver;
constexpr int kNoDepthCopy = static_cast<int>(DepthCopyMethod::None);
constexpr int kReszDepthCopy = static_cast<int>(DepthCopyMethod::Resz);
constexpr UINT kWidth = 1280;
constexpr UINT kHeight = 720;
constexpr UINT kWorldHeight = 688;
constexpr float kNearQuadDepth = 0.25f;
constexpr float kFarQuadDepth = 0.75f;
constexpr float kQuadTop = 600.0f;
constexpr float kQuadBottom = 680.0f;
constexpr DepthTexel kNearQuadTexel = {320, 640};
constexpr DepthTexel kFarQuadTexel = {960, 640};
constexpr float kCopiedDepthTolerance = 1e-6f;
constexpr double kMinFogLumaChange = 0.01;
constexpr DWORD kClientDepthFunction = D3DCMP_GREATEREQUAL;
constexpr float kSilhouetteX = 640.0f;
constexpr float kBeyondViewport = 1.0f;
constexpr float kNearSilhouetteYards = 5.0f;
constexpr float kFarBackgroundYards = 400.0f;
constexpr DWORD kSilhouetteSceneColour = 0xFF202020;
constexpr DWORD kCoverageInside = 0xFFFFFFFF;
constexpr DWORD kCoverageOutside = 0xFF000000;
constexpr int kSilhouetteSearchFrom = 630;
constexpr int kSilhouetteSearchTo = 650;
constexpr int kReferenceOffset = 4;
constexpr int kSilhouetteRowStep = 40;
constexpr int kMinPartialCoverage = 50;
constexpr int kMaxPartialCoverage = 235;
constexpr int kMinFogContrast = 30;
constexpr float kMinCoverageBlend = 0.1f;
constexpr float kMaxCoverageBlend = 0.9f;
constexpr int kMinSilhouetteRows = 8;
constexpr int kSilhouetteMargin = 3;
constexpr int kMaxSingleSampledDifference = 2;
constexpr float kMinShadedWaterFraction = 0.9f;
constexpr int kWaterEdgeMargin = 2;
constexpr int kMaxCopiedSampleOffsetWaterDifference = 6;
constexpr D3DFORMAT kListDisplayFormat = D3DFMT_X8R8G8B8;
constexpr D3DFORMAT kListDepthFormats[] = {D3DFMT_D16, D3DFMT_D24X8, D3DFMT_D24S8, D3DFMT_D32};
constexpr int kListSampleCounts[] = {0, 2, 4, 6, 8, 10, 12, 14, 16};
constexpr double kMaxListCostFactor = 3.0;
constexpr double kListCostAllowanceMs = 10.0;

struct Targets
{
    D3DSURFACE_DESC backBuffer = {};
    D3DSURFACE_DESC depth = {};
};

Config MultisamplingConfig(bool keep)
{
    Config cfg;
    cfg.overlay = false;
    cfg.multisampling = keep;
    cfg.noiseAmount = 0.0f;
    cfg.temporal = 0.0f;
    cfg.godRays = 0.0f;
    return cfg;
}

MultisamplingStatus CurrentStatus()
{
    MultisamplingStatus status;
    vf_test_multisampling(&status);
    return status;
}

bool Kept(const MultisamplingStatus& status)
{
    return status.method && *status.method && status.samples == kFourSamples;
}

bool OffBecause(const MultisamplingStatus& status, const char* reason)
{
    return !(status.method && *status.method) && status.off && std::strcmp(status.off, reason) == 0;
}

bool DriverCanCopyDepth(IDirect3D9* real)
{
    D3DADAPTER_IDENTIFIER9 id = {};
    D3DDISPLAYMODE mode = {};
    real->GetAdapterIdentifier(0, 0, &id);
    real->GetAdapterDisplayMode(0, &mode);
    return id.VendorId == kNvidiaVendorId ||
           SUCCEEDED(real->CheckDeviceFormat(0, D3DDEVTYPE_HAL, mode.Format, D3DUSAGE_RENDERTARGET, D3DRTYPE_SURFACE,
                                             kResz));
}

bool DriverOffersResz(IDirect3D9* real)
{
    D3DDISPLAYMODE mode = {};
    real->GetAdapterDisplayMode(0, &mode);
    return SUCCEEDED(
        real->CheckDeviceFormat(0, D3DDEVTYPE_HAL, mode.Format, D3DUSAGE_RENDERTARGET, D3DRTYPE_SURFACE, kResz));
}

void PrepareParameters(Harness& m, D3DMULTISAMPLE_TYPE samples, D3DFORMAT depthFormat)
{
    m.pp = {};
    m.pp.Windowed = TRUE;
    m.pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    m.pp.BackBufferWidth = kWidth;
    m.pp.BackBufferHeight = kHeight;
    m.pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    m.pp.MultiSampleType = samples;
    m.pp.EnableAutoDepthStencil = TRUE;
    m.pp.AutoDepthStencilFormat = depthFormat;
    m.pp.hDeviceWindow = m.window;
    m.pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
}

bool OpenDevice(Harness& m, D3DMULTISAMPLE_TYPE samples, D3DFORMAT depthFormat = kClientDepthFormat)
{
    PrepareParameters(m, samples, depthFormat);
    const HRESULT hr = m.d3d->CreateDevice(0, D3DDEVTYPE_HAL, m.window, kClientDeviceFlags, &m.pp, &m.dev);
    if (FAILED(hr) || !m.dev)
    {
        m.dev = nullptr;
        return false;
    }
    m.CreateEngineObjects();
    return true;
}

void CloseDevice(Harness& m)
{
    if (!m.dev)
        return;
    m.ReleaseEngineObjects();
    m.dev->Release();
    m.dev = nullptr;
}

bool ResetTo(Harness& m, D3DMULTISAMPLE_TYPE samples)
{
    m.ReleaseEngineObjects();
    m.pp.MultiSampleType = samples;
    m.pp.MultiSampleQuality = 0;
    const HRESULT hr = m.dev->Reset(&m.pp);
    m.CreateEngineObjects();
    return SUCCEEDED(hr);
}

Targets DescribeTargets(IDirect3DDevice9* dev)
{
    Targets targets;
    IDirect3DSurface9* backBuffer = nullptr;
    IDirect3DSurface9* depth = nullptr;
    if (SUCCEEDED(dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)))
    {
        backBuffer->GetDesc(&targets.backBuffer);
        backBuffer->Release();
    }
    if (SUCCEEDED(dev->GetDepthStencilSurface(&depth)) && depth)
    {
        depth->GetDesc(&targets.depth);
        depth->Release();
    }
    return targets;
}

bool KeepsMultisampledTargets(const Targets& t)
{
    return t.backBuffer.MultiSampleType == kFourSamples && t.depth.MultiSampleType == kFourSamples &&
           t.depth.Format == kStencilDepthFormat;
}

bool BindsSingleSampledIntz(const Targets& t)
{
    return t.backBuffer.MultiSampleType == D3DMULTISAMPLE_NONE && t.depth.MultiSampleType == D3DMULTISAMPLE_NONE &&
           t.depth.Format == kIntz;
}

void PrintTargets(const char* when, const Targets& t, const MultisamplingStatus& status)
{
    const bool kept = status.method && *status.method;
    std::printf("     %s: back buffer %d samples, depth format 0x%08X with %d samples; multisampling %s%s\n", when,
                t.backBuffer.MultiSampleType, static_cast<unsigned>(t.depth.Format), t.depth.MultiSampleType,
                kept ? "kept, depth copied by " : "off: ", kept ? status.method : status.off);
}

struct WorldCamera
{
    Vec3 eye = Add({0, 0, 9}, kGameLikeWorldOffset);
    Vec3 at = Add({100, 2, 4}, kGameLikeWorldOffset);
    D3DVIEWPORT9 world = {0, 0, kWidth, kWorldHeight, 0.0f, 1.0f};
    float view[16] = {};
    float proj[16] = {};

    WorldCamera()
    {
        EngineProjection(static_cast<float>(kWidth) / kWorldHeight, proj);
        CameraRelativeLookAt(eye, at, view);
    }

    FrameInputs Inputs() const { return MakeInputs(view, proj, eye, at, world); }

    float RawDepthAt(float viewYards) const
    {
        float d3dProj[16];
        RemapToD3DDepthRange(proj, d3dProj);
        return d3dProj[10] + d3dProj[14] / viewYards;
    }
};

const water_checks::ClientRenderState kClientStencil[] = {
    {D3DRS_STENCILFUNC, D3DCMP_LESS},       {D3DRS_STENCILREF, 7},
    {D3DRS_STENCILMASK, 0x3C},              {D3DRS_STENCILWRITEMASK, 0x5A},
    {D3DRS_STENCILPASS, D3DSTENCILOP_INCR}, {D3DRS_STENCILFAIL, D3DSTENCILOP_DECR},
    {D3DRS_STENCILZFAIL, D3DSTENCILOP_INVERT},
};

bool ClientStencilKept(IDirect3DDevice9* dev)
{
    bool kept = true;
    for (const water_checks::ClientRenderState& state : kClientStencil)
    {
        DWORD value = 0;
        dev->GetRenderState(state.state, &value);
        kept = kept && value == state.value;
    }
    return kept;
}

struct FogFrame
{
    bool rendered = false;
    const char* skip = "";
    bool statesKept = false;
    bool stencilKept = false;
    DWORD depthFunction = 0;
    bool depthRead = false;
    float copied[2] = {};
    double lumaChange = 0.0;
};

FogFrame RenderFogOverDepthQuads(Harness& m)
{
    const WorldCamera camera;
    FogFrame f;
    m.BeginFrame();
    m.DrawScene(camera.eye, camera.view, camera.proj, camera.world);
    m.DrawPretransformedQuadAtRawDepth(40.0f, kQuadTop, 600.0f, kQuadBottom, kNearQuadDepth);
    m.DrawPretransformedQuadAtRawDepth(680.0f, kQuadTop, 1240.0f, kQuadBottom, kFarQuadDepth);
    const Image before = Capture(m.dev);
    m.SetEngineState(camera.world);
    m.dev->SetRenderState(D3DRS_ZFUNC, kClientDepthFunction);
    for (const water_checks::ClientRenderState& state : kClientStencil)
        m.dev->SetRenderState(state.state, state.value);
    Sentinel s0;
    ReadSentinel(m.dev, s0);
    const FrameInputs in = camera.Inputs();
    f.rendered = vf_test_render(&in, &f.skip) != 0;
    Sentinel s1;
    ReadSentinel(m.dev, s1);
    f.statesKept = SameSentinel(s0, s1);
    if (!f.statesKept)
        ReportSentinelDifferences(s0, s1);
    ReleaseSentinel(s0);
    ReleaseSentinel(s1);
    m.dev->GetRenderState(D3DRS_ZFUNC, &f.depthFunction);
    f.stencilKept = ClientStencilKept(m.dev);
    const DepthTexel texels[2] = {kNearQuadTexel, kFarQuadTexel};
    f.depthRead = vf_test_read_scene_depth(texels, 2, f.copied) != 0;
    const Image after = Capture(m.dev);
    f.lumaChange = MeanLumaChange(before, after, 0, 0, kWidth, kWorldHeight);
    m.dev->EndScene();
    m.dev->Present(nullptr, nullptr, nullptr, nullptr);
    return f;
}

bool CopiedDepthMatches(const FogFrame& f)
{
    return f.depthRead && std::fabs(f.copied[0] - kNearQuadDepth) <= kCopiedDepthTolerance &&
           std::fabs(f.copied[1] - kFarQuadDepth) <= kCopiedDepthTolerance;
}

void PrintFogFrame(const char* when, const FogFrame& f)
{
    std::printf("     %s: fog %s%s, copied depth %.7f / %.7f (drawn %.2f / %.2f), mean luma change %.4f\n", when,
                f.rendered ? "drawn" : "skipped: ", f.rendered ? "" : f.skip, f.copied[0], f.copied[1],
                kNearQuadDepth, kFarQuadDepth, f.lumaChange);
}

bool FogFrameDrawn(const FogFrame& f)
{
    return f.rendered && CopiedDepthMatches(f) && f.lumaChange > kMinFogLumaChange;
}

void CheckVideoOptionsOffer(Harness& m, IDirect3D9* real, bool driverCopies)
{
    DWORD wrappedLevels = 0;
    DWORD realLevels = 0;
    Config keep = MultisamplingConfig(true);
    vf_test_set_config(&keep);
    const HRESULT offered =
        m.d3d->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, FALSE, kFourSamples, &wrappedLevels);
    const HRESULT native =
        real->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, FALSE, kFourSamples, &realLevels);
    const HRESULT depthOffered =
        m.d3d->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, D3DFMT_D24S8, FALSE, kFourSamples, nullptr);
    std::printf("     4x in the game's Video options: wrapper 0x%08lX (%lu quality levels), driver 0x%08lX (%lu)\n",
                offered, wrappedLevels, native, realLevels);
    if (driverCopies)
        Check(SUCCEEDED(native) && offered == native && wrappedLevels == realLevels && SUCCEEDED(depthOffered),
              "with Multisampling=1 and a depth copy method, CheckDeviceMultiSampleType passes 4x through for the "
              "game's colour and depth formats");
    else
        Check(offered == D3DERR_NOTAVAILABLE, "without a depth copy method the wrapper still hides multisampling");

    Config off = MultisamplingConfig(false);
    vf_test_set_config(&off);
    const HRESULT hidden =
        m.d3d->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, FALSE, kFourSamples, nullptr);
    vf_test_set_config(&keep);
    vf_test_force_depth_copy_method(kNoDepthCopy);
    const HRESULT noMethod =
        m.d3d->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, FALSE, kFourSamples, nullptr);
    vf_test_force_depth_copy_method(kDepthCopyFromDriver);
    const HRESULT single =
        m.d3d->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, FALSE, D3DMULTISAMPLE_NONE, nullptr);
    Check(hidden == D3DERR_NOTAVAILABLE && noMethod == D3DERR_NOTAVAILABLE && SUCCEEDED(single),
          "Multisampling=0 or no depth copy method hides 4x from the game's Video options and keeps 1x");
}

struct ListCost
{
    int calls = 0;
    double milliseconds = 0.0;
};

ListCost BuildVideoOptionsMultisampleList(IDirect3D9* d3d)
{
    ListCost cost;
    LARGE_INTEGER start = {};
    LARGE_INTEGER end = {};
    QueryPerformanceCounter(&start);
    const UINT modes = d3d->GetAdapterModeCount(0, kListDisplayFormat);
    for (UINT i = 0; i < modes; ++i)
    {
        D3DDISPLAYMODE mode = {};
        d3d->EnumAdapterModes(0, kListDisplayFormat, i, &mode);
        for (const D3DFORMAT depth : kListDepthFormats)
        {
            if (FAILED(d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, mode.Format, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE,
                                              depth)))
                continue;
            for (const int count : kListSampleCounts)
            {
                const auto samples = static_cast<D3DMULTISAMPLE_TYPE>(count);
                DWORD levels = 0;
                d3d->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, mode.Format, FALSE, samples, &levels);
                d3d->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, depth, FALSE, samples, &levels);
                cost.calls += 2;
            }
        }
    }
    QueryPerformanceCounter(&end);
    cost.milliseconds = water_fft_checks::MillisecondsBetween(start, end);
    return cost;
}

void CheckVideoOptionsListCost(Harness& m, IDirect3D9* real)
{
    Config keep = MultisamplingConfig(true);
    vf_test_set_config(&keep);
    const ListCost direct = BuildVideoOptionsMultisampleList(real);
    const ListCost wrapped = BuildVideoOptionsMultisampleList(m.d3d);
    std::printf("     the Video options' multisample list: %d CheckDeviceMultiSampleType calls, %.1f ms through the "
                "wrapper, %.1f ms on the driver\n",
                wrapped.calls, wrapped.milliseconds, direct.milliseconds);
    Check(wrapped.calls > 0 && wrapped.milliseconds <= kMaxListCostFactor * direct.milliseconds + kListCostAllowanceMs,
          "building the game's multisample list through the wrapper costs about what the driver's own checks cost "
          "(the depth copy probe runs once per adapter)");
}

void CheckSingleSampledFallback(Harness& m, const char* what, const char* reason)
{
    const bool opened = OpenDevice(m, kFourSamples);
    const Targets targets = opened ? DescribeTargets(m.dev) : Targets();
    const MultisamplingStatus status = CurrentStatus();
    PrintTargets(what, targets, status);
    FogFrame frame;
    if (opened)
        frame = RenderFogOverDepthQuads(m);
    Check(opened && m.pp.MultiSampleType == D3DMULTISAMPLE_NONE && BindsSingleSampledIntz(targets) &&
              OffBecause(status, reason) && FogFrameDrawn(frame),
          (std::string(what) + ": the device is created single-sampled with INTZ bound, as before, and the fog "
                               "draws")
              .c_str());
    CloseDevice(m);
}

void CheckFallbacks(Harness& m, IDirect3D9* real)
{
    Config off = MultisamplingConfig(false);
    vf_test_set_config(&off);
    CheckSingleSampledFallback(m, "Multisampling=0 with 4x requested", "Multisampling=0 in CoAVolFog.ini");

    Config keep = MultisamplingConfig(true);
    vf_test_set_config(&keep);
    vf_test_force_depth_copy_method(kNoDepthCopy);
    CheckSingleSampledFallback(m, "no depth copy method with 4x requested",
                               "no depth copy method (forced by the harness)");
    if (!DriverOffersResz(real))
    {
        const size_t logStart = water_settings_checks::DllLogSize();
        vf_test_force_depth_copy_method(kReszDepthCopy);
        CheckSingleSampledFallback(m, "RESZ forced on a driver without it", "the depth copy self-test failed");
        const std::string log = runtime_cost::LogWrittenSince(logStart);
        Check(runtime_cost::HasLine(log, "depth copy self-test: RESZ failed") &&
                  runtime_cost::HasLine(log, "ms requested 4 (quality 0) used 0") &&
                  runtime_cost::HasLine(log, "multisampling off: the depth copy self-test failed"),
              "a failed depth copy self-test is logged with the requested and used sample counts and the reason");
    }
    vf_test_force_depth_copy_method(kDepthCopyFromDriver);
}

bool IsFullWater(const Image& mask, UINT x, UINT y)
{
    return water_checks::IsMaskPixel(mask, x, y);
}

bool IsDry(const Image& mask, const Image& stock, UINT x, UINT y)
{
    return std::memcmp(mask.At(x, y), stock.At(x, y), 3) == 0;
}

bool FullWaterAround(const Image& mask, UINT x, UINT y, int margin)
{
    for (int dy = -margin; dy <= margin; ++dy)
        for (int dx = -margin; dx <= margin; ++dx)
        {
            const int px = static_cast<int>(x) + dx;
            const int py = static_cast<int>(y) + dy;
            if (px < 0 || py < 0 || px >= static_cast<int>(mask.w) || py >= static_cast<int>(mask.h) ||
                !IsFullWater(mask, static_cast<UINT>(px), static_cast<UINT>(py)))
                return false;
        }
    return true;
}

bool SameSceneAround(const Image& a, const Image& b, UINT x, UINT y, int margin)
{
    for (int dy = -margin; dy <= margin; ++dy)
        for (int dx = -margin; dx <= margin; ++dx)
        {
            const int px = static_cast<int>(x) + dx;
            const int py = static_cast<int>(y) + dy;
            if (px < 0 || py < 0 || px >= static_cast<int>(a.w) || py >= static_cast<int>(a.h) ||
                std::memcmp(a.At(static_cast<UINT>(px), static_cast<UINT>(py)),
                            b.At(static_cast<UINT>(px), static_cast<UINT>(py)), 3) != 0)
                return false;
        }
    return true;
}

int LargestChannelDifference(const Image& a, const Image& b, UINT x, UINT y)
{
    int largest = 0;
    for (int ch = 0; ch < 3; ++ch)
        largest = std::max(largest, std::abs(a.At(x, y)[ch] - b.At(x, y)[ch]));
    return largest;
}

struct WaterFrames
{
    water_checks::WaterFrameResult stock;
    water_checks::WaterFrameResult mask;
    water_checks::WaterFrameResult shaded;
};

WaterFrames RenderWaterFrames(Harness& m)
{
    const Config optics = water_checks::OpticsOnlyConfig(MultisamplingConfig(true));
    vf_test_set_config(&optics);
    water_checks::BasinClient client(m, water_checks::DefaultWaterView());
    water_checks::WaterFrame none;
    none.calls = water_checks::WaterCalls::None;
    water_checks::WaterFrame mask = none;
    mask.opaqueMask = true;
    water_checks::WaterFrame pass;
    WaterFrames frames;
    frames.stock = client.Render(none);
    frames.mask = client.Render(mask);
    frames.shaded = client.Render(pass);
    return frames;
}

void CheckWaterOnMultisampledTargets(const WaterFrames& frames, const std::wstring& outDir)
{
    const Image& mask = frames.mask.image;
    const Image& stock = frames.stock.image;
    const Image& shaded = frames.shaded.image;
    water_checks::SaveImage(outDir, L"msaa-water-shaded", shaded);
    size_t full = 0;
    size_t fullChanged = 0;
    size_t edges = 0;
    size_t edgesChanged = 0;
    size_t dryChanged = 0;
    for (UINT y = 0; y < shaded.h; ++y)
        for (UINT x = 0; x < shaded.w; ++x)
        {
            const bool changed = std::memcmp(shaded.At(x, y), stock.At(x, y), 3) != 0;
            if (IsFullWater(mask, x, y))
            {
                ++full;
                fullChanged += changed ? 1 : 0;
            }
            else if (IsDry(mask, stock, x, y))
                dryChanged += changed ? 1 : 0;
            else
            {
                ++edges;
                edgesChanged += changed ? 1 : 0;
            }
        }
    std::printf("     4x water: %zu covered pixels (%zu reshaded), %zu partly covered edge pixels (%zu reshaded), "
                "%zu dry pixels changed\n",
                full, fullChanged, edges, edgesChanged, dryChanged);
    Check(frames.shaded.began && frames.shaded.stateKept && frames.shaded.armWritesDocumented &&
              frames.shaded.tagWritesDocumented && frames.shaded.untagRestores,
          (std::string("on a 4x device the water pass arms, tags the multisampled stencil and restores the state ") +
           frames.shaded.skip)
              .c_str());
    Check(full > 0 && fullChanged >= kMinShadedWaterFraction * full && dryChanged == 0 && edges > 0 &&
              edgesChanged > 0,
          "on a 4x device the tagged water samples are reshaded, water edges blend by coverage and no dry pixel "
          "changes");
}

void CheckWaterMatchesSingleSampled(const WaterFrames& multisampled, const WaterFrames& single)
{
    size_t compared = 0;
    int largest = 0;
    for (UINT y = 0; y < multisampled.shaded.image.h; ++y)
        for (UINT x = 0; x < multisampled.shaded.image.w; ++x)
            if (FullWaterAround(multisampled.mask.image, x, y, kWaterEdgeMargin) &&
                FullWaterAround(single.mask.image, x, y, kWaterEdgeMargin) &&
                SameSceneAround(multisampled.stock.image, single.stock.image, x, y, kWaterEdgeMargin))
            {
                ++compared;
                const int difference = LargestChannelDifference(multisampled.shaded.image, single.shaded.image, x, y);
                largest = std::max(largest, difference);
            }
    std::printf("     flat water away from its and the scene's edges: %zu pixels, largest 4x vs 1x difference "
                "%d/255\n",
                compared, largest);
    Check(single.shaded.began && compared > 0 && largest <= kMaxCopiedSampleOffsetWaterDifference,
          "on a 4x device water refraction and absorption use copies of the scene and water-surface depth equal to "
          "the single-sampled ones");
}

Image RenderSilhouette(Harness& m, const WorldCamera& camera, DWORD nearColour, DWORD farColour, bool fog)
{
    m.BeginFrame();
    m.dev->SetViewport(&camera.world);
    m.dev->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
    m.dev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    m.dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    m.dev->SetRenderState(D3DRS_LIGHTING, FALSE);
    m.dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
    m.dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m.dev->SetVertexShader(nullptr);
    m.dev->SetPixelShader(nullptr);
    m.dev->SetTexture(0, nullptr);
    m.dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    m.dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    const float right = static_cast<float>(kWidth) + kBeyondViewport;
    const float bottom = static_cast<float>(kWorldHeight) + kBeyondViewport;
    m.DrawPretransformedQuadAtRawDepth(-kBeyondViewport, -kBeyondViewport, right, bottom,
                                       camera.RawDepthAt(kFarBackgroundYards), farColour);
    m.DrawPretransformedQuadAtRawDepth(-kBeyondViewport, -kBeyondViewport, kSilhouetteX, bottom,
                                       camera.RawDepthAt(kNearSilhouetteYards), nearColour);
    if (fog)
    {
        const char* skip = "";
        const FrameInputs in = camera.Inputs();
        if (!vf_test_render(&in, &skip))
            std::printf("     fog skipped: %s\n", skip);
    }
    Image image = Capture(m.dev);
    m.dev->EndScene();
    m.dev->Present(nullptr, nullptr, nullptr, nullptr);
    return image;
}

struct SilhouetteFrames
{
    Image coverage;
    Image overSceneCopy;
    Image fixedFunctionBlend;
};

SilhouetteFrames RenderSilhouetteFrames(Harness& m)
{
    const WorldCamera camera;
    SilhouetteFrames frames;
    Config linear = MultisamplingConfig(true);
    vf_test_set_config(&linear);
    frames.coverage = RenderSilhouette(m, camera, kCoverageInside, kCoverageOutside, false);
    frames.overSceneCopy = RenderSilhouette(m, camera, kSilhouetteSceneColour, kSilhouetteSceneColour, true);
    Config gamma = linear;
    gamma.colorSpace = 0;
    vf_test_set_config(&gamma);
    frames.fixedFunctionBlend = RenderSilhouette(m, camera, kSilhouetteSceneColour, kSilhouetteSceneColour, true);
    vf_test_set_config(&linear);
    return frames;
}

int PartlyCoveredColumn(const Image& coverage, UINT y)
{
    for (int x = kSilhouetteSearchFrom; x <= kSilhouetteSearchTo; ++x)
    {
        const int value = coverage.At(static_cast<UINT>(x), y)[1];
        if (value >= kMinPartialCoverage && value <= kMaxPartialCoverage)
            return x;
    }
    return -1;
}

void CheckSilhouetteFogBlendsByCoverage(const Image& coverage, const Image& fogged, const char* blend)
{
    int rows = 0;
    int blended = 0;
    float lowest = 1.0f;
    float highest = 0.0f;
    for (UINT y = kSilhouetteRowStep; y < kWorldHeight - kSilhouetteRowStep; y += kSilhouetteRowStep)
    {
        const int x = PartlyCoveredColumn(coverage, y);
        if (x < 0)
            continue;
        int channel = 0;
        int contrast = 0;
        const UINT nearX = static_cast<UINT>(x - kReferenceOffset);
        const UINT farX = static_cast<UINT>(x + kReferenceOffset);
        for (int ch = 0; ch < 3; ++ch)
        {
            const int span = fogged.At(nearX, y)[ch] - fogged.At(farX, y)[ch];
            if (std::abs(span) > std::abs(contrast))
            {
                contrast = span;
                channel = ch;
            }
        }
        if (std::abs(contrast) < kMinFogContrast)
            continue;
        ++rows;
        const float farValue = fogged.At(farX, y)[channel];
        const float share = (fogged.At(static_cast<UINT>(x), y)[channel] - farValue) / contrast;
        lowest = std::min(lowest, share);
        highest = std::max(highest, share);
        blended += share >= kMinCoverageBlend && share <= kMaxCoverageBlend ? 1 : 0;
    }
    std::printf("     %s: silhouette rows with partial coverage and fog contrast %d, blended by coverage %d "
                "(near-fog share %.2f..%.2f)\n",
                blend, rows, blended, lowest, highest);
    Check(rows >= kMinSilhouetteRows && blended == rows,
          (std::string("at a 4x silhouette the pixel blends the near and the far fog by sample coverage (") + blend +
           ")")
              .c_str());
}

void CheckSilhouetteFogBlendsByCoverage(const SilhouetteFrames& frames, const std::wstring& outDir)
{
    water_checks::SaveImage(outDir, L"msaa-silhouette", frames.overSceneCopy);
    CheckSilhouetteFogBlendsByCoverage(frames.coverage, frames.overSceneCopy, "linear light over the scene copy");
    CheckSilhouetteFogBlendsByCoverage(frames.coverage, frames.fixedFunctionBlend, "gamma, fixed-function blend");
}

int LargestDifferenceAwayFromSilhouette(const Image& multisampled, const Image& single)
{
    int largest = 0;
    for (UINT y = 0; y < kWorldHeight; ++y)
        for (UINT x = 0; x < kWidth; ++x)
            if (std::abs(static_cast<int>(x) - static_cast<int>(kSilhouetteX)) > kSilhouetteMargin)
                largest = std::max(largest, LargestChannelDifference(multisampled, single, x, y));
    return largest;
}

void CheckSilhouetteMatchesSingleSampled(const SilhouetteFrames& multisampled, const SilhouetteFrames& single)
{
    const int linear = LargestDifferenceAwayFromSilhouette(multisampled.overSceneCopy, single.overSceneCopy);
    const int gamma = LargestDifferenceAwayFromSilhouette(multisampled.fixedFunctionBlend, single.fixedFunctionBlend);
    std::printf("     fogged frame away from the silhouette: largest 4x vs 1x difference %d/255 over the scene copy, "
                "%d/255 with fixed-function blending\n",
                linear, gamma);
    Check(linear <= kMaxSingleSampledDifference && gamma <= kMaxSingleSampledDifference,
          "away from silhouettes the 4x fog equals the single-sampled fog in both blend modes");
}

void CheckMultisampledDevice(Harness& m, const std::wstring& outDir)
{
    Config keep = MultisamplingConfig(true);
    vf_test_set_config(&keep);
    const size_t logStart = water_settings_checks::DllLogSize();
    const bool opened = OpenDevice(m, kFourSamples);
    const Targets created = opened ? DescribeTargets(m.dev) : Targets();
    const MultisamplingStatus status = CurrentStatus();
    PrintTargets("4x requested", created, status);
    Check(opened && m.pp.MultiSampleType == kFourSamples && m.pp.EnableAutoDepthStencil == TRUE &&
              KeepsMultisampledTargets(created) && Kept(status),
          "with Multisampling=1 CreateDevice keeps the game's 4x back buffer and a 4x automatic depth");
    Check(opened && m.pp.AutoDepthStencilFormat == kClientDepthFormat &&
              runtime_cost::HasLine(runtime_cost::LogWrittenSince(logStart), "depth requested D24X8 used D24S8"),
          "the game's D24X8 depth has no stencil for the water tags and the silhouette split, so the 4x depth is "
          "created as D24S8, logged, and the game still sees its own format");
    if (!opened)
        return;
    D3DDEVICE_CREATION_PARAMETERS cp = {};
    m.dev->GetCreationParameters(&cp);
    Check((cp.BehaviorFlags & D3DCREATE_PUREDEVICE) == 0, "the multisampled device drops the pure-device flag");

    const FogFrame fog = RenderFogOverDepthQuads(m);
    PrintFogFrame("4x fog", fog);
    Check(FogFrameDrawn(fog), "fog draws on a 4x device and its INTZ copy equals the depth the scene drew");
    Check(fog.statesKept && fog.depthFunction == kClientDepthFunction && fog.stencilKept,
          "on a 4x device the fog restores render, sampler, shader, constant, target, depth-test and stencil state");

    water_checks::AssignWaterData(water_checks::MakeSyntheticWaterData());
    vf_test_set_water_seconds(water_checks::kFrameSeconds);
    const WaterFrames multisampledWater = RenderWaterFrames(m);
    CheckWaterOnMultisampledTargets(multisampledWater, outDir);
    const SilhouetteFrames multisampledSilhouette = RenderSilhouetteFrames(m);
    CheckSilhouetteFogBlendsByCoverage(multisampledSilhouette, outDir);

    const bool single = ResetTo(m, D3DMULTISAMPLE_NONE);
    const Targets reset1x = DescribeTargets(m.dev);
    const MultisamplingStatus status1x = CurrentStatus();
    PrintTargets("Reset to 1x", reset1x, status1x);
    const FogFrame fog1x = RenderFogOverDepthQuads(m);
    PrintFogFrame("1x fog", fog1x);
    Check(single && m.pp.MultiSampleType == D3DMULTISAMPLE_NONE && BindsSingleSampledIntz(reset1x) &&
              OffBecause(status1x, "the game's Multisampling option is 1x") && FogFrameDrawn(fog1x),
          "Reset from 4x to 1x binds INTZ again and the fog draws");
    const WaterFrames singleWater = RenderWaterFrames(m);
    CheckWaterMatchesSingleSampled(multisampledWater, singleWater);
    CheckSilhouetteMatchesSingleSampled(multisampledSilhouette, RenderSilhouetteFrames(m));

    const size_t resetLogStart = water_settings_checks::DllLogSize();
    const bool again = ResetTo(m, kFourSamples);
    const std::string resetLog = runtime_cost::LogWrittenSince(resetLogStart);
    const Targets reset4x = DescribeTargets(m.dev);
    const MultisamplingStatus status4x = CurrentStatus();
    PrintTargets("Reset to 4x", reset4x, status4x);
    const FogFrame fog4x = RenderFogOverDepthQuads(m);
    PrintFogFrame("4x fog after Reset", fog4x);
    Check(again && m.pp.MultiSampleType == kFourSamples && KeepsMultisampledTargets(reset4x) && Kept(status4x) &&
              FogFrameDrawn(fog4x),
          "Reset from 1x back to 4x keeps the game's multisampling and the fog draws on a fresh depth copy");
    Check(m.pp.AutoDepthStencilFormat == kClientDepthFormat &&
              runtime_cost::HasLine(resetLog, "depth requested D24X8 used D24S8"),
          "Reset to 4x also creates the game's D24X8 depth as D24S8, logs it and hands the game its own format");
    vf_test_set_water_seconds(water_checks::kRealTime);
    CloseDevice(m);
}

void CheckStencilLessDepthFormat(Harness& m, D3DFORMAT format, const char* name)
{
    Config keep = MultisamplingConfig(true);
    vf_test_set_config(&keep);
    const bool opened = OpenDevice(m, kFourSamples, format);
    const Targets targets = opened ? DescribeTargets(m.dev) : Targets();
    const MultisamplingStatus status = CurrentStatus();
    const std::string requested = std::string("4x with ") + name + " requested";
    PrintTargets(requested.c_str(), targets, status);
    FogFrame frame;
    if (opened)
        frame = RenderFogOverDepthQuads(m);
    PrintFogFrame(requested.c_str(), frame);
    Check(opened && KeepsMultisampledTargets(targets) && Kept(status) && m.pp.AutoDepthStencilFormat == format &&
              FogFrameDrawn(frame),
          (std::string("with a ") + name + " depth requested, 4x is kept on a D24S8 depth and the fog draws").c_str());
    CloseDevice(m);
}

void CheckMultisamplingSetting(const std::wstring& outDir, const std::wstring& shippedIni)
{
    ConfigStore shipped;
    shipped.Load(NarrowPath(shippedIni));
    const std::vector<std::string> lines = water_settings_checks::IniLines(ReadText(shippedIni));
    const std::string text = ReadText(shippedIni);
    Check(shipped.Get().multisampling && std::count(lines.begin(), lines.end(), std::string("Multisampling=1")) == 1 &&
              text.find("; 1 = keep the game's Multisampling video option") != std::string::npos,
          "the shipped INI keeps the game's multisampling (Multisampling=1) and documents the key");

    const std::wstring savedIni = FullPath(outDir + L"\\multisampling.ini");
    CopyFileW(shippedIni.c_str(), savedIni.c_str(), FALSE);
    WritePrivateProfileStringW(L"CoAVolFog", L"Multisampling", L"0", savedIni.c_str());
    ConfigStore store;
    store.Load(NarrowPath(savedIni));
    const bool readOff = !store.Get().multisampling;
    Config edited = store.Get();
    edited.multisampling = true;
    const std::string changes = SettingChanges(store.Get(), edited);
    store.Apply(edited);
    const bool saved = store.Save();
    ConfigStore reloaded;
    reloaded.Load(NarrowPath(savedIni));
    std::printf("     Multisampling=0 read as %d; the window's change logs \"%s\"\n", readOff ? 0 : 1,
                changes.c_str());
    Check(readOff && saved && reloaded.Get().multisampling && changes == "Multisampling 0 -> 1" &&
              ReadText(savedIni).find("\nMultisampling=1") != std::string::npos,
          "Multisampling=0 is read from the INI, and the settings window's change is logged and saved");
}

void CheckMultisampling(Direct3DCreate realCreate, HWND window, const std::wstring& outDir,
                        const std::wstring& shippedIni)
{
    CheckMultisamplingSetting(outDir, shippedIni);
    IDirect3D9* real = realCreate(D3D_SDK_VERSION);
    Harness m;
    m.window = window;
    m.clearFlags = kClientClearFlags;
    m.d3d = vf_test_wrap_direct3d9(realCreate, D3D_SDK_VERSION);
    Check(real && m.d3d, "a second wrapped Direct3D9 for the multisampling checks");
    if (!real || !m.d3d)
        return;
    const bool driverCopies = DriverCanCopyDepth(real);
    D3DADAPTER_IDENTIFIER9 id = {};
    real->GetAdapterIdentifier(0, 0, &id);
    std::printf("     adapter vendor 0x%04lX: %s\n", id.VendorId,
                driverCopies ? "a depth copy method is expected (NVAPI or RESZ)" : "no depth copy method expected");
    CheckVideoOptionsOffer(m, real, driverCopies);
    CheckVideoOptionsListCost(m, real);
    CheckFallbacks(m, real);
    if (driverCopies)
    {
        CheckMultisampledDevice(m, outDir);
        CheckStencilLessDepthFormat(m, D3DFMT_D16, "D16");
    }
    Config shipped = {};
    vf_test_set_config(&shipped);
    const ULONG refs = m.d3d->Release();
    real->Release();
    Check(refs == 0, "the multisampling checks release every wrapper reference");
}
}
