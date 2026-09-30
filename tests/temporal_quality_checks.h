#include "ps_temporal.h"
#include "ps_history_depth.h"
#include "ps_ray_mask.h"
#include "vs_fullscreen.h"

namespace temporal_quality
{
constexpr UINT kSize = 4;
constexpr UINT kRayDepthSize = 32;
constexpr float kFogDistance = 100.0f;
constexpr float kCurrentFog = 0.2f;
constexpr float kHistoryFog = 0.8f;
constexpr float kWeight = 0.75f;
constexpr float kSameSurfaceAverage = (7.0f * kCurrentFog + 1.0f) / 9.0f;

DWORD Grey(float value)
{
    const auto channel = static_cast<DWORD>(std::lround(value * 255.0f));
    return channel * 0x01010101u;
}

DWORD PackedDepth(float viewZ, DWORD depthClass)
{
    const auto packed = static_cast<DWORD>(std::lround(viewZ / kFogDistance * 65535.0f));
    return 0xFF000000u | ((packed >> 8) << 16) | ((packed & 255u) << 8) | depthClass;
}

bool Fill(IDirect3DTexture9* texture, DWORD colour, int changedIndex = -1, DWORD changedColour = 0)
{
    D3DLOCKED_RECT locked = {};
    if (FAILED(texture->LockRect(0, &locked, nullptr, 0)))
        return false;
    for (UINT y = 0; y < kSize; ++y)
    {
        auto* row = reinterpret_cast<DWORD*>(static_cast<BYTE*>(locked.pBits) + y * locked.Pitch);
        for (UINT x = 0; x < kSize; ++x)
            row[x] = static_cast<int>(y * kSize + x) == changedIndex ? changedColour : colour;
    }
    return SUCCEEDED(texture->UnlockRect(0));
}

struct Fixture
{
    IDirect3DDevice9* dev;
    IDirect3DStateBlock9* state = nullptr;
    IDirect3DSurface9* savedTarget = nullptr;
    IDirect3DSurface9* savedDepth = nullptr;
    IDirect3DVertexBuffer9* savedStream = nullptr;
    UINT savedOffset = 0;
    UINT savedStride = 0;
    IDirect3DTexture9* inputs[4] = {};
    IDirect3DTexture9* output = nullptr;
    IDirect3DTexture9* rayDepth = nullptr;
    IDirect3DSurface9* target = nullptr;
    IDirect3DSurface9* readback = nullptr;
    IDirect3DVertexShader9* vs = nullptr;
    IDirect3DPixelShader9* ps = nullptr;
    IDirect3DPixelShader9* depthPs = nullptr;
    IDirect3DPixelShader9* rayPs = nullptr;
    IDirect3DVertexDeclaration9* declaration = nullptr;
    float reproject[16] = {};
    DWORD encodedResult = 0;
    UINT size;

    explicit Fixture(IDirect3DDevice9* device, UINT textureSize = kSize) : dev(device), size(textureSize) {}

    ~Fixture()
    {
        if (savedTarget)
            dev->SetRenderTarget(0, savedTarget);
        dev->SetDepthStencilSurface(savedDepth);
        if (state)
            state->Apply();
        dev->SetStreamSource(0, savedStream, savedOffset, savedStride);
        IUnknown* resources[] = {state, savedTarget, savedDepth, savedStream, inputs[0], inputs[1], inputs[2],
                                 inputs[3], output, rayDepth, target, readback, vs, ps, depthPs, rayPs, declaration};
        for (IUnknown* resource : resources)
            if (resource)
                resource->Release();
    }

    bool Create()
    {
        if (FAILED(dev->CreateStateBlock(D3DSBT_ALL, &state)))
            return false;
        dev->GetRenderTarget(0, &savedTarget);
        dev->GetDepthStencilSurface(&savedDepth);
        dev->GetStreamSource(0, &savedStream, &savedOffset, &savedStride);
        for (auto*& texture : inputs)
            if (FAILED(dev->CreateTexture(size, size, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, nullptr)))
                return false;
        static const D3DVERTEXELEMENT9 elements[] = {
            {0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0}, D3DDECL_END()};
        if (FAILED(dev->CreateTexture(size, size, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                      D3DPOOL_DEFAULT, &output, nullptr)) ||
            FAILED(output->GetSurfaceLevel(0, &target)) ||
            FAILED(dev->CreateOffscreenPlainSurface(size, size, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM,
                                                    &readback, nullptr)) ||
            FAILED(dev->CreateVertexShader(reinterpret_cast<const DWORD*>(g_vs_fullscreen), &vs)) ||
            FAILED(dev->CreatePixelShader(reinterpret_cast<const DWORD*>(g_ps_temporal), &ps)) ||
            FAILED(dev->CreatePixelShader(reinterpret_cast<const DWORD*>(g_ps_history_depth), &depthPs)) ||
            FAILED(dev->CreatePixelShader(reinterpret_cast<const DWORD*>(g_ps_ray_mask), &rayPs)) ||
            FAILED(dev->CreateTexture(kRayDepthSize, kRayDepthSize, 1, 0, D3DFMT_A8R8G8B8,
                                      D3DPOOL_MANAGED, &rayDepth, nullptr)) ||
            FAILED(dev->CreateVertexDeclaration(elements, &declaration)))
            return false;
        return true;
    }

    bool Reset()
    {
        std::memset(reproject, 0, sizeof(reproject));
        reproject[0] = reproject[5] = reproject[11] = 1.0f;
        if (!Fill(inputs[0], Grey(kCurrentFog), 0, Grey(0.0f)) ||
            !Fill(inputs[1], Grey(kHistoryFog)) || !Fill(inputs[2], Grey(128.0f / 255.0f)) ||
            !Fill(inputs[3], PackedDepth(1.0f, 0)))
            return false;
        D3DLOCKED_RECT locked = {};
        if (FAILED(inputs[0]->LockRect(0, &locked, nullptr, 0)))
            return false;
        auto* row = reinterpret_cast<DWORD*>(static_cast<BYTE*>(locked.pBits) + 2 * locked.Pitch);
        row[2] = Grey(1.0f);
        return SUCCEEDED(inputs[0]->UnlockRect(0));
    }

    float Draw(bool historyValid = true, float captureDepth = 0.0f, bool adaptiveLighting = false)
    {
        const float extent = static_cast<float>(size);
        float constants[14][4] = {
            {0, 0, extent, extent},
            {1, 0, 1.0f / extent, 1.0f / extent},
            {1, 1, 0, 0},
            {1, 128.0f / 255.0f - 1.0f, kFogDistance, 0.94f},
            {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1},
            {extent, extent, 1.0f / extent, 1.0f / extent},
            {}, {}, {}, {},
            {kWeight, historyValid ? 1.0f : 0.0f, adaptiveLighting ? 1.0f : 0.0f, 0},
        };
        std::memcpy(constants[9], reproject, sizeof(reproject));
        if (captureDepth > 0.0f)
        {
            constants[3][1] *= captureDepth;
            constants[3][2] = 5000.0f;
        }
        dev->SetDepthStencilSurface(nullptr);
        dev->SetRenderTarget(0, target);
        D3DVIEWPORT9 viewport = {0, 0, size, size, 0.0f, 1.0f};
        dev->SetViewport(&viewport);
        dev->SetVertexShader(vs);
        dev->SetPixelShader(captureDepth > 0.0f ? depthPs : ps);
        dev->SetVertexDeclaration(declaration);
        dev->SetStreamSourceFreq(0, 1);
        dev->SetRenderState(D3DRS_ZENABLE, FALSE);
        dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
        dev->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
        dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
        dev->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
        dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        for (DWORD stage = 0; stage < 4; ++stage)
        {
            dev->SetTexture(stage, inputs[captureDepth > 0.0f && stage == 0 ? 2 : stage]);
            dev->SetSamplerState(stage, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            dev->SetSamplerState(stage, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
            dev->SetSamplerState(stage, D3DSAMP_MINFILTER, D3DTEXF_POINT);
            dev->SetSamplerState(stage, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
            dev->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
            dev->SetSamplerState(stage, D3DSAMP_SRGBTEXTURE, FALSE);
        }
        dev->SetPixelShaderConstantF(0, &constants[0][0], 14);
        return DrawConfigured();
    }

    float DrawConfigured()
    {
        const float triangle[3][4] = {{-1, -1, 0, 1}, {-1, 3, 0, 1}, {3, -1, 0, 1}};
        if (FAILED(dev->BeginScene()))
            return -1.0f;
        const HRESULT drawn = dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, triangle, sizeof(triangle[0]));
        const HRESULT ended = dev->EndScene();
        if (FAILED(drawn) || FAILED(ended) || FAILED(dev->GetRenderTargetData(target, readback)))
            return -1.0f;
        D3DLOCKED_RECT locked = {};
        if (FAILED(readback->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
            return -1.0f;
        const auto* row = reinterpret_cast<const DWORD*>(static_cast<const BYTE*>(locked.pBits) + locked.Pitch);
        encodedResult = row[1];
        const float result = static_cast<float>((encodedResult >> 16) & 255u) / 255.0f;
        readback->UnlockRect();
        return result;
    }

    bool ReadRed(std::vector<float>& red)
    {
        D3DLOCKED_RECT locked = {};
        if (FAILED(readback->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
            return false;
        red.resize(size * size);
        for (UINT y = 0; y < size; ++y)
        {
            const auto* row = reinterpret_cast<const DWORD*>(static_cast<const BYTE*>(locked.pBits) + y * locked.Pitch);
            for (UINT x = 0; x < size; ++x)
                red[y * size + x] = static_cast<float>((row[x] >> 16) & 255u) / 255.0f;
        }
        return SUCCEEDED(readback->UnlockRect());
    }

    float DrawRayMask(bool skyAtSample)
    {
        if (Draw(false) < 0.0f || !Fill(inputs[1], Grey(1.0f)))
            return -1.0f;
        D3DLOCKED_RECT locked = {};
        if (FAILED(rayDepth->LockRect(0, &locked, nullptr, 0)))
            return -1.0f;
        for (UINT y = 0; y < kRayDepthSize; ++y)
        {
            auto* row = reinterpret_cast<DWORD*>(static_cast<BYTE*>(locked.pBits) + y * locked.Pitch);
            for (UINT x = 0; x < kRayDepthSize; ++x)
                row[x] = Grey((y == 12) == skyAtSample ? 1.0f : 0.5f);
        }
        if (FAILED(rayDepth->UnlockRect(0)))
            return -1.0f;
        const float viewport[4] = {3, 5, 17, 19};
        const float pixelGrid[4] = {1, 0, 1.0f / kRayDepthSize, 1.0f / kRayDepthSize};
        const float mask[2][4] = {{0, 0, 1, 0}, {17.0f / kSize, 0, 1.0f / kSize, 1.0f / kSize}};
        dev->SetPixelShader(rayPs);
        dev->SetPixelShaderConstantF(0, viewport, 1);
        dev->SetPixelShaderConstantF(1, pixelGrid, 1);
        dev->SetPixelShaderConstantF(9, &mask[0][0], 2);
        dev->SetTexture(0, rayDepth);
        dev->SetTexture(1, inputs[1]);
        return DrawConfigured();
    }
};

bool NearTemporal(float actual, float expected)
{
    if (std::fabs(actual - expected) > 2.0f / 255.0f)
    {
        std::printf("     temporal value %.4f expected %.4f\n", actual, expected);
        return false;
    }
    return true;
}

bool CapturesDepth(Fixture& fixture, float viewDepth, DWORD rawDepth, DWORD expectedClass)
{
    if (!Fill(fixture.inputs[2], rawDepth) || fixture.Draw(true, viewDepth) < 0.0f)
        return false;
    const DWORD red = (fixture.encodedResult >> 16) & 255u;
    const DWORD green = (fixture.encodedResult >> 8) & 255u;
    const int depthClass = static_cast<int>(fixture.encodedResult & 255u);
    const double restoredDepth = (red * 256u + green) * (5000.0 / 65535.0);
    const double maximumError = 0.5 * 5000.0 / 65535.0;
    const bool matches = std::fabs(restoredDepth - viewDepth) <= maximumError &&
                         std::abs(depthClass - static_cast<int>(expectedClass)) <= 1;
    if (!matches)
        std::printf("     depth history %.6f yd class %d expected %.6f yd class %lu\n",
                    restoredDepth, depthClass, viewDepth, expectedClass);
    return matches;
}

constexpr UINT kNoiseSize = 16;
constexpr UINT kForegroundColumns = 8;
constexpr float kNoiseMean = 0.5f;
constexpr float kNoiseAmplitude = 0.25f;
constexpr float kHistoryNoiseAmplitude = 0.35f;
constexpr float kForegroundFog = 0.9f;
constexpr float kBackgroundMean = 0.3f;
constexpr float kBackgroundAmplitude = 0.1f;
constexpr float kBackgroundRawDepth = 0.9f;
constexpr float kMaxFallbackNoiseRatio = 0.5f;

float Jitter(UINT x, UINT y, uint32_t seed)
{
    uint32_t hash = (x * 73856093u) ^ (y * 19349663u) ^ (seed * 83492791u);
    hash ^= hash >> 13;
    hash *= 0x5BD1E995u;
    hash ^= hash >> 15;
    return static_cast<float>(hash & 0xFFFFu) / 65535.0f * 2.0f - 1.0f;
}

template <typename Texel>
bool FillTexels(IDirect3DTexture9* texture, UINT size, Texel texel)
{
    D3DLOCKED_RECT locked = {};
    if (FAILED(texture->LockRect(0, &locked, nullptr, 0)))
        return false;
    for (UINT y = 0; y < size; ++y)
    {
        auto* row = reinterpret_cast<DWORD*>(static_cast<BYTE*>(locked.pBits) + y * locked.Pitch);
        for (UINT x = 0; x < size; ++x)
            row[x] = texel(x, y);
    }
    return SUCCEEDED(texture->UnlockRect(0));
}

float Decoded(DWORD grey)
{
    return static_cast<float>(grey & 255u) / 255.0f;
}

DWORD NoisyCurrent(UINT x, UINT y)
{
    return Grey(kNoiseMean + kNoiseAmplitude * Jitter(x, y, 1));
}

DWORD NoisyHistory(UINT x, UINT y)
{
    return Grey(kNoiseMean + kHistoryNoiseAmplitude * Jitter(x, y, 2));
}

struct Spread
{
    double mean = 0;
    double deviation = 0;
};

Spread SpreadOf(const std::vector<float>& values)
{
    Spread spread;
    for (float value : values)
        spread.mean += value;
    spread.mean /= values.size();
    for (float value : values)
        spread.deviation += (value - spread.mean) * (value - spread.mean);
    spread.deviation = std::sqrt(spread.deviation / values.size());
    return spread;
}

std::vector<float> NoisyCurrentValues()
{
    std::vector<float> values;
    for (UINT y = 0; y < kNoiseSize; ++y)
        for (UINT x = 0; x < kNoiseSize; ++x)
            values.push_back(Decoded(NoisyCurrent(x, y)));
    return values;
}

bool ResetNoisy(Fixture& fixture)
{
    std::memset(fixture.reproject, 0, sizeof(fixture.reproject));
    fixture.reproject[0] = fixture.reproject[5] = fixture.reproject[11] = 1.0f;
    return FillTexels(fixture.inputs[0], kNoiseSize, NoisyCurrent) &&
           FillTexels(fixture.inputs[1], kNoiseSize, NoisyHistory) &&
           FillTexels(fixture.inputs[2], kNoiseSize, [](UINT, UINT) { return Grey(128.0f / 255.0f); }) &&
           FillTexels(fixture.inputs[3], kNoiseSize, [](UINT, UINT) { return PackedDepth(1.0f, 0); });
}

UINT ClampedTexel(UINT coordinate, int offset)
{
    return static_cast<UINT>(std::clamp(static_cast<int>(coordinate) + offset, 0, static_cast<int>(kNoiseSize) - 1));
}

float ClampAndBlendHistory(UINT x, UINT y)
{
    const float current = Decoded(NoisyCurrent(x, y));
    float lowest = current;
    float highest = current;
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
        {
            const float neighbour = Decoded(NoisyCurrent(ClampedTexel(x, dx), ClampedTexel(y, dy)));
            lowest = std::min(lowest, neighbour);
            highest = std::max(highest, neighbour);
        }
    const float history = std::clamp(Decoded(NoisyHistory(x, y)), lowest, highest);
    return current + (history - current) * kWeight;
}

bool FallbackCutsNoise(Fixture& fixture, bool historyValid, const char* path)
{
    std::vector<float> output;
    if (fixture.Draw(historyValid) < 0.0f || !fixture.ReadRed(output))
        return false;
    const Spread input = SpreadOf(NoisyCurrentValues());
    const Spread filtered = SpreadOf(output);
    const double ratio = filtered.deviation / input.deviation;
    std::printf("     %s: fog noise %.4f -> %.4f (ratio %.2f), mean %.4f -> %.4f\n", path, input.deviation,
                filtered.deviation, ratio, input.mean, filtered.mean);
    return ratio < kMaxFallbackNoiseRatio && std::fabs(filtered.mean - input.mean) <= 2.0 / 255.0;
}

DWORD SplitCurrent(UINT x, UINT y)
{
    return x < kForegroundColumns ? Grey(kForegroundFog)
                                  : Grey(kBackgroundMean + kBackgroundAmplitude * Jitter(x, y, 3));
}

DWORD SplitDepth(UINT x, UINT)
{
    return Grey(x < kForegroundColumns ? 128.0f / 255.0f : kBackgroundRawDepth);
}

DWORD RejectedHistoryDepth(UINT, UINT)
{
    return PackedDepth(10.0f, 0);
}

bool AveragesOwnSurface(Fixture& fixture)
{
    std::vector<float> output;
    if (!ResetNoisy(fixture) || !FillTexels(fixture.inputs[0], kNoiseSize, SplitCurrent) ||
        !FillTexels(fixture.inputs[2], kNoiseSize, SplitDepth) ||
        !FillTexels(fixture.inputs[3], kNoiseSize, RejectedHistoryDepth) || fixture.Draw() < 0.0f ||
        !fixture.ReadRed(output))
        return false;
    const float brightestBackground = kBackgroundMean + kBackgroundAmplitude + 1.0f / 255.0f;
    bool separated = true;
    for (UINT y = 0; y < kNoiseSize; ++y)
        separated = separated &&
                    std::fabs(output[y * kNoiseSize + kForegroundColumns - 1] - kForegroundFog) <= 1.0f / 255.0f &&
                    output[y * kNoiseSize + kForegroundColumns] <= brightestBackground;
    return separated;
}

bool HistoryPathUnchanged(Fixture& fixture)
{
    std::vector<float> output;
    if (!ResetNoisy(fixture) || fixture.Draw() < 0.0f || !fixture.ReadRed(output))
        return false;
    bool unchanged = true;
    uint32_t checksum = 0;
    for (UINT y = 0; y < kNoiseSize; ++y)
        for (UINT x = 0; x < kNoiseSize; ++x)
        {
            const float value = output[y * kNoiseSize + x];
            checksum = checksum * 31u + static_cast<uint32_t>(std::lround(value * 255.0f));
            unchanged = unchanged && std::fabs(value - ClampAndBlendHistory(x, y)) <= 1.0f / 255.0f + 1.0e-4f;
        }
    std::printf("     history path output checksum %08X\n", checksum);
    return unchanged;
}
}

void CheckTemporalFallbackNoise(IDirect3DDevice9* dev)
{
    using namespace temporal_quality;
    Fixture fixture(dev, kNoiseSize);
    const bool ready = fixture.Create() && ResetNoisy(fixture);
    Check(ready, "temporal fallback regression resources created");
    if (!ready)
        return;
    Check(HistoryPathUnchanged(fixture),
          "temporal history on a matching surface still blends the history clamped to the current 3x3 range");
    Check(ResetNoisy(fixture) && FillTexels(fixture.inputs[3], kNoiseSize, RejectedHistoryDepth) &&
              FallbackCutsNoise(fixture, true, "disocclusion"),
          "temporal disocclusion averages the current jitter over the surface instead of showing one frame's noise");
    Check(ResetNoisy(fixture) && FallbackCutsNoise(fixture, false, "invalid history"),
          "temporal invalid history (camera cut, settings change) averages the current jitter");
    Check(ResetNoisy(fixture), "temporal fallback fixture resets");
    fixture.reproject[8] = 10.0f;
    Check(FallbackCutsNoise(fixture, true, "off-screen"), "temporal screen-edge reveal averages the current jitter");
    Check(ResetNoisy(fixture), "temporal fallback fixture resets behind the camera");
    fixture.reproject[11] = -1.0f;
    Check(FallbackCutsNoise(fixture, true, "behind the camera"),
          "temporal reprojection behind the camera averages the current jitter");
    Check(AveragesOwnSurface(fixture),
          "the temporal fallback averages only current samples on the pixel's own surface");
}

void CheckTemporalQuality(IDirect3DDevice9* dev)
{
    using namespace temporal_quality;
    Fixture fixture(dev);
    const bool ready = fixture.Create() && fixture.Reset();
    Check(ready, "temporal regression shader resources created");
    if (!ready)
        return;
    const float accumulated = kCurrentFog * (1 - kWeight) + kHistoryFog * kWeight;
    Check(NearTemporal(fixture.Draw(), accumulated), "temporal matching surface reuses history");
    Check(NearTemporal(fixture.Draw(true, 0, true), kCurrentFog),
          "moving light darkening rejects radiance history despite matching depth and neighbourhood range");
    Check(Fill(fixture.inputs[1], Grey(0.22f)) &&
              NearTemporal(fixture.Draw(true, 0, true), kCurrentFog * (1 - kWeight) + 0.22f * kWeight),
          "stable lighting retains the configured history weight for small radiance differences");
    Check(Fill(fixture.inputs[1], Grey(kCurrentFog / 0.7f)) &&
              NearTemporal(fixture.Draw(true, 0, true), kCurrentFog + (kCurrentFog / 0.7f - kCurrentFog) *
                                                                          kWeight * 0.5f),
          "temporal lighting rejection fades continuously through intermediate radiance changes");
    Check(Fill(fixture.inputs[0], Grey(kCurrentFog), 5, Grey(kHistoryFog)) &&
              Fill(fixture.inputs[1], Grey(kCurrentFog)) &&
              NearTemporal(fixture.Draw(true, 0, true), kHistoryFog),
          "moving light brightening rejects stale radiance at the same surface depth");
    fixture.Reset();
    fixture.reproject[15] = 1.0f;
    Check(Fill(fixture.inputs[3], PackedDepth(2.0f, 0)) && NearTemporal(fixture.Draw(), accumulated),
          "temporal camera translation validates distance in the previous view");
    fixture.Reset();
    Check(Fill(fixture.inputs[3], PackedDepth(10.0f, 0)) && NearTemporal(fixture.Draw(), kSameSurfaceAverage),
          "temporal newly exposed geometry rejects history at another depth");
    Check(Fill(fixture.inputs[3], PackedDepth(1.0f, 255)) && NearTemporal(fixture.Draw(), kSameSurfaceAverage),
          "temporal geometry rejects sky history even at matching depth");
    fixture.Reset();
    fixture.reproject[8] = 0.25f;
    Check(Fill(fixture.inputs[1], Grey(0.4f), 6, Grey(1.0f)) &&
              Fill(fixture.inputs[3], PackedDepth(1.0f, 0), 6, PackedDepth(10.0f, 0)) &&
              NearTemporal(fixture.Draw(), kCurrentFog * (1 - kWeight) + 0.4f * kWeight),
          "temporal bilinear history excludes an occluded neighbour before filtering");
    fixture.Reset();
    Check(Fill(fixture.inputs[2], Grey(1.0f)) && Fill(fixture.inputs[3], PackedDepth(50.0f, 255)) &&
              NearTemporal(fixture.Draw(), accumulated),
          "temporal sky history remains usable without a finite surface distance");
    Check(Fill(fixture.inputs[3], PackedDepth(kFogDistance, 128)) &&
              NearTemporal(fixture.Draw(), kSameSurfaceAverage),
          "temporal sky rejects distant terrain history");
    fixture.Reset();
    Check(NearTemporal(fixture.Draw(false), kSameSurfaceAverage),
          "temporal invalid history returns the same-surface average of the current samples");
    fixture.reproject[8] = 10.0f;
    Check(NearTemporal(fixture.Draw(), kSameSurfaceAverage),
          "temporal off-screen reprojection returns the same-surface average of the current samples");
    fixture.Reset();
    fixture.reproject[11] = -1.0f;
    Check(NearTemporal(fixture.Draw(), kSameSurfaceAverage),
          "temporal reprojection behind the camera returns the same-surface average of the current samples");
    fixture.Reset();
    for (float viewDepth : {0.4f, 1.0f, 100.0f, 4999.0f})
    {
        char label[120];
        std::snprintf(label, sizeof(label), "depth history captures %.1f yd within half a 16-bit depth interval",
                      viewDepth);
        Check(CapturesDepth(fixture, viewDepth, Grey(128.0f / 255.0f), 0), label);
    }
    Check(CapturesDepth(fixture, 5000.0f, Grey(0.98f), 128),
          "depth history distinguishes distant terrain at the maximum fog distance");
    Check(CapturesDepth(fixture, 5000.0f, Grey(1.0f), 255),
          "depth history distinguishes clear sky from distant terrain");
    fixture.Reset();
    Check(NearTemporal(fixture.DrawRayMask(true), 1.0f),
          "god-ray mask samples sky at the matching odd subviewport position");
    Check(NearTemporal(fixture.DrawRayMask(false), 0.0f),
          "god-ray mask rejects geometry at the matching odd subviewport position");
}
