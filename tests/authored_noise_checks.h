#pragma once

#include "ps_lit_noisy_march_high.h"
#include "ps_lit_noisy_march_low.h"
#include "ps_lit_noisy_march_mid.h"
#include "ps_noisy_march_high.h"
#include "ps_noisy_march_low.h"
#include "ps_noisy_march_mid.h"

namespace authored_noise
{
constexpr int kCurveSamples = 1000;
constexpr float kForeverCurveGain = 1.0f / 18.0f;
constexpr float kForeverCurveLowerPole = 19.0f / 36.0f;
constexpr float kForeverCurveUpperPole = 17.0f / 36.0f;
constexpr float kCurveTolerance = 1e-5f;
constexpr float kCurveAtFourTenths = 0.0696f;
constexpr float kPivotSlope = 20.0f;
constexpr UINT kProbeSide = 8;
constexpr float kProbeSampleTolerance = 0.004f;
constexpr float kProbeCurveTolerance = 1e-4f;
constexpr float kMarchTolerance = 2.0f / 255.0f;
constexpr float kSilentNoiseTolerance = 1.0f / 255.0f;
constexpr float kLayerLength = 200.0f;
constexpr float kLayerDensity = 0.01f;
constexpr float kFullStorm = 1.0f;
constexpr float kNoiselessTile = 300.0f;
constexpr double kMaxVolumeBuildMilliseconds = 40.0;
constexpr float kMarchLength = 1000.0f;
constexpr double kProbedPixelRay = 0.125;
const float kWorldOrigin[3] = {};
const float kShiftedCamera[3] = {37.0f, -53.0f, 211.0f};
constexpr float kCameraShiftSensitivity = 10.0f / 255.0f;
constexpr UINT kRampSide = 4;
constexpr UINT kRampDepth = 64;
constexpr double kRampMean = 127.5;
constexpr double kRampAmplitude = 89.25;
constexpr double kTwoPi = 6.283185307179586;
constexpr BYTE kThinningNoise = 102;
const float kHyjalSummit[3] = {5458.0f, -2934.0f, 1481.0f};
constexpr Vec3 kToMoonOverHyjal = {0.63f, 0.63f, 0.455f};

float ForeverNoiseCurve(float x)
{
    return x < 0.5f ? x * x * -kForeverCurveGain / (x - kForeverCurveLowerPole)
                    : 1.0f + (1.0f - x) * (1.0f - x) * kForeverCurveGain / (kForeverCurveUpperPole - x);
}

void CheckCurveMatchesForever()
{
    float worst = 0.0f;
    bool monotonic = true;
    float previous = -1.0f;
    for (int i = 0; i <= kCurveSamples; ++i)
    {
        const float x = static_cast<float>(i) / kCurveSamples;
        const float y = NoiseDensityCurve(x);
        worst = std::fmax(worst, std::fabs(y - ForeverNoiseCurve(x)));
        monotonic = monotonic && y >= previous;
        previous = y;
    }
    const float slope = (NoiseDensityCurve(0.5005f) - NoiseDensityCurve(0.4995f)) / 0.001f;
    std::printf("     noise curve: largest difference from Forever's constants %.2g, curve(0.4) %.4f, slope at the "
                "pivot %.2f\n",
                worst, NoiseDensityCurve(0.4f), slope);
    Check(worst < kCurveTolerance && monotonic && NoiseDensityCurve(0.0f) == 0.0f &&
              Near(NoiseDensityCurve(0.5f), 0.5f) && NoiseDensityCurve(1.0f) == 1.0f &&
              std::fabs(NoiseDensityCurve(0.4f) - kCurveAtFourTenths) < 5e-4f && std::fabs(slope - kPivotSlope) < 0.5f,
          "the noise density curve is Forever's contrast-20 S-curve through (0.5, 0.5)");
}

double MillisecondsToPrepareTheVolume()
{
    LARGE_INTEGER frequency = {};
    LARGE_INTEGER before = {};
    LARGE_INTEGER after = {};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&before);
    PrepareAuthoredNoise();
    QueryPerformanceCounter(&after);
    return 1000.0 * static_cast<double>(after.QuadPart - before.QuadPart) / static_cast<double>(frequency.QuadPart);
}

void CheckVolumeIsBalancedAndTileable()
{
    const double buildMilliseconds = MillisecondsToPrepareTheVolume();
    std::printf("     authored noise volume built in %.1f ms\n", buildMilliseconds);
    Check(buildMilliseconds < kMaxVolumeBuildMilliseconds,
          "the authored noise volume builds in a few milliseconds, too fast to stall a frame");
    const UINT n = kAuthoredNoiseSize;
    double sum = 0.0;
    double squares = 0.0;
    double curve = 0.0;
    double interiorStep = 0.0;
    double seamStep = 0.0;
    for (UINT z = 0; z < n; ++z)
        for (UINT y = 0; y < n; ++y)
            for (UINT x = 0; x < n; ++x)
            {
                const double value = AuthoredNoiseTexel(x, y, z) / 255.0;
                sum += value;
                squares += value * value;
                curve += NoiseDensityCurve(static_cast<float>(value));
                const int texel = AuthoredNoiseTexel(x, y, z);
                const int steps[3] = {std::abs(AuthoredNoiseTexel(x + 1, y, z) - texel),
                                      std::abs(AuthoredNoiseTexel(x, y + 1, z) - texel),
                                      std::abs(AuthoredNoiseTexel(x, y, z + 1) - texel)};
                const UINT coordinates[3] = {x, y, z};
                for (int axis = 0; axis < 3; ++axis)
                    (coordinates[axis] == n - 1 ? seamStep : interiorStep) += steps[axis];
            }
    const double count = static_cast<double>(n) * n * n;
    const double mean = sum / count;
    const double spread = std::sqrt(squares / count - mean * mean);
    const double meanCurve = curve / count;
    const double meanInteriorStep = interiorStep / (3.0 * count * (n - 1) / n);
    const double meanSeamStep = seamStep / (3.0 * count / n);
    std::printf("     authored noise volume: mean %.4f, spread %.4f, mean curve %.4f; neighbour step %.2f inside, %.2f "
                "across the tile seam\n",
                mean, spread, meanCurve, meanInteriorStep, meanSeamStep);
    Check(std::fabs(mean - 0.5) < 0.01 && spread > 0.06 && std::fabs(meanCurve - kNoiseCurveMean) < 0.02,
          "the authored noise volume is centred on the curve's pivot, so its mean density factor is 1 - alpha/2");
    Check(meanSeamStep < 1.5 * meanInteriorStep, "the authored noise volume tiles without a seam");
}

FrameInputs HyjalMidnight()
{
    const Vec3 hyjal = {kHyjalSummit[0], kHyjalSummit[1], kHyjalSummit[2]};
    return ContinentFrame(kKalimdor, hyjal, 0.0f, kToMoonOverHyjal, true);
}

void CheckModelTakesAuthoredNoise(const FogData& data)
{
    const FrameInputs hyjal = HyjalMidnight();
    AuthoredFog fog = {};
    const bool resolved = data.Resolve(kKalimdor, hyjal.camPos, hyjal.dayFraction, kClearWeather, fog);
    Config on;
    Config off;
    off.classicNoise = false;
    const FogParams noisy = BuildFogParams(hyjal, on, &fog);
    const FogParams quiet = BuildFogParams(hyjal, off, &fog);
    const LayerNoise& haze = noisy.noise[0];
    std::printf("     Hyjal haze noise: alpha %.2f, weights %.2f/%.2f, tiles %.0f/%.0f yd; distance fog density %.6f "
                "with noise, %.6f without\n",
                haze.alpha, haze.octaveWeight[0], haze.octaveWeight[1], 1.0f / haze.inverseTileYards[0],
                1.0f / haze.inverseTileYards[1], noisy.layers[kDistanceFogLayer].density,
                quiet.layers[kDistanceFogLayer].density);
    Check(resolved && haze.alpha == 1.0f && Near(haze.octaveWeight[0], 0.5f) && Near(haze.octaveWeight[1], 0.5f) &&
              Near(haze.inverseTileYards[0] * 5000.0f, 1.0f) && Near(haze.velocity[1][2], -30.0f) &&
              noisy.layers[0].densityVariation == 0.0f && noisy.noise[1].alpha == 0.0f &&
              noisy.layers[1].densityVariation == 1.0f,
          "a Classic layer with authored noise takes its parameters instead of the artistic variation");
    Check(quiet.noise[0].alpha == 0.0f && quiet.layers[0].densityVariation == 1.0f && !AnyLayerNoise(quiet),
          "ClassicNoise=0 turns the authored noise off and restores the artistic variation");
    Check(noisy.layers[kDistanceFogLayer].density > quiet.layers[kDistanceFogLayer].density,
          "the distance fog that hides the far clip counts noisy layers at their mean density");

    FrameInputs sea = ContinentFrame(kEasternKingdoms, {kOpenSeaWestOfElwynn[0], kOpenSeaWestOfElwynn[1],
                                                        kOpenSeaWestOfElwynn[2]},
                                     kNoon, {0.2f, 0.2f, 0.95f}, false);
    sea.lightParams = Storm(0.5f);
    AuthoredFog halfStorm = {};
    data.Resolve(kEasternKingdoms, sea.camPos, sea.dayFraction, sea.lightParams, halfStorm);
    const FogParams half = BuildFogParams(sea, on, &halfStorm);
    Check(Near(half.noise[2].alpha, 0.5f) && Near(half.layers[2].densityVariation, 0.5f),
          "a half-weight noise layer keeps half the artistic variation, so neither switches at a blend");

    LayerNoise full = {};
    full.alpha = 1.0f;
    full.fade[0] = 1.0f;
    FogLayer layer = {};
    layer.density = 0.01f;
    layer.emissive[0] = 0.25f;
    layer.shadowEmissive[0] = 0.25f;
    const FogLayer mean = MeanNoiseLayer(layer, full, 1.0f);
    const FogLayer untouched = MeanNoiseLayer(layer, LayerNoise{}, 1.0f);
    Check(Near(MeanNoiseDensity(full), 0.5f) && Near(mean.density, 0.005f) && Near(mean.emissive[0], 0.25f) &&
              std::memcmp(&untouched, &layer, sizeof(layer)) == 0,
          "where the noise is not sampled a layer takes its mean: half the density, fade colour hidden in the gaps");
}

double PositiveFraction(double value)
{
    return value - std::floor(value);
}

double CircularDistance(double a, double b)
{
    const double d = std::fabs(a - b);
    return std::fmin(d, 1.0 - d);
}

double UploadedTileCoordinate(const float* octaveRegister, const double* position, int axis)
{
    return PositiveFraction((position[axis] - octaveRegister[axis]) * octaveRegister[3]);
}

void CheckScrollDriftsWithinOneTile()
{
    FogParams fog = {};
    LayerNoise& noise = fog.noise[1];
    noise.alpha = 1.0f;
    noise.inverseTileYards[0] = 1.0f / 300.0f;
    noise.inverseTileYards[1] = 1.0f / 250.0f;
    const float velocity[2][3] = {{15.0f, -7.0f, 2.0f}, {-30.0f, 4.0f, 0.5f}};
    std::memcpy(noise.velocity, velocity, sizeof(velocity));
    const float camera[3] = {-8576.0f, 1007.0f, 104.0f};
    AuthoredNoiseScroll scroll;
    LayerNoiseRegisters start[kSceneLayers];
    scroll.Advance(fog, camera, 0.0);
    scroll.Registers(fog, start);
    const double seconds[] = {0.016, 1000.5, 3600.0};
    for (double step : seconds)
        scroll.Advance(fog, camera, step);
    LayerNoiseRegisters end[kSceneLayers];
    scroll.Registers(fog, end);
    const double elapsed = 0.016 + 1000.5 + 3600.0;
    bool wrapped = true;
    double worst = 0.0;
    const double positions[][3] = {{-8576.0, 1007.0, 104.0}, {5458.0, -2934.0, 1481.0}, {12.5, -0.25, 0.0}};
    for (int octave = 0; octave < kAuthoredNoiseOctaves; ++octave)
    {
        const float* before = start[1].octaveOffsetAndInverseTile[octave];
        const float* after = end[1].octaveOffsetAndInverseTile[octave];
        const double tile = 1.0 / noise.inverseTileYards[octave];
        for (int axis = 0; axis < 3; ++axis)
        {
            wrapped = wrapped && after[axis] >= 0.0f && after[axis] <= tile;
            for (const double* p : positions)
            {
                const double drifted[3] = {p[0] + velocity[octave][0] * elapsed, p[1] + velocity[octave][1] * elapsed,
                                           p[2] + velocity[octave][2] * elapsed};
                worst = std::fmax(worst, CircularDistance(UploadedTileCoordinate(before, p, axis),
                                                          UploadedTileCoordinate(after, drifted, axis)));
            }
        }
    }
    std::printf("     noise scroll after %.0f s: pattern off its drift by at most %.2g of a tile\n", elapsed, worst);
    Check(wrapped && worst < 1e-5 && end[1].fadeAndAlpha[3] == 1.0f &&
              end[0].octaveOffsetAndInverseTile[0][3] == 0.0f,
          "noise scroll moves the pattern by direction x speed x time and uploads offsets within one tile");
}

constexpr int kWeatherBlendSteps = 100;
constexpr double kWeatherBlendStepSeconds = 0.05;
constexpr double kHour = 3600.0;
constexpr double kMaxBlendPathTiles = 0.1;
constexpr double kPathGrowthTolerance = 1e-3;

double HyjalStormBlendPath(const FogData& data, double clearSeconds, float* tiles)
{
    const FrameInputs hyjal = HyjalMidnight();
    const double pointNearCamera[3] = {hyjal.camPos[0] + 212.0, hyjal.camPos[1] + 212.0, hyjal.camPos[2]};
    const Config config;
    AuthoredNoiseScroll scroll;
    double coordinates[kAuthoredNoiseOctaves][3] = {};
    double path[kAuthoredNoiseOctaves][3] = {};
    for (int step = -1; step <= kWeatherBlendSteps; ++step)
    {
        const float storm = static_cast<float>(std::max(step, 0)) / kWeatherBlendSteps;
        AuthoredFog fog = {};
        data.Resolve(kKalimdor, hyjal.camPos, hyjal.dayFraction, Storm(storm), fog);
        const FogParams params = BuildFogParams(hyjal, config, &fog);
        scroll.Advance(params, hyjal.camPos, step < 0 ? clearSeconds : kWeatherBlendStepSeconds);
        LayerNoiseRegisters registers[kSceneLayers];
        scroll.Registers(params, registers);
        for (int octave = 0; octave < kAuthoredNoiseOctaves; ++octave)
        {
            const float* uploaded = registers[0].octaveOffsetAndInverseTile[octave];
            tiles[(step < 0 ? 0 : kAuthoredNoiseOctaves) + octave] = uploaded[3] > 0.0f ? 1.0f / uploaded[3] : 0.0f;
            for (int axis = 0; axis < 3; ++axis)
            {
                const double coordinate = UploadedTileCoordinate(uploaded, pointNearCamera, axis);
                if (step >= 0)
                    path[octave][axis] += CircularDistance(coordinates[octave][axis], coordinate);
                coordinates[octave][axis] = coordinate;
            }
        }
    }
    double longest = 0.0;
    for (const auto& octave : path)
        for (double axis : octave)
            longest = std::fmax(longest, axis);
    return longest;
}

void CheckStormKeepsTheScrollAcrossTileBlends(const FogData& data)
{
    float tiles[2 * kAuthoredNoiseOctaves] = {};
    const double afterASecond = HyjalStormBlendPath(data, 1.0, tiles);
    const double afterAnHour = HyjalStormBlendPath(data, kHour, tiles);
    const double afterThreeHours = HyjalStormBlendPath(data, 3.0 * kHour, tiles);
    std::printf("     Hyjal haze tiles %.0f/%.0f yd clear, %.0f/%.0f yd in a storm; a 5 s storm blend moves the noise "
                "300 yd from the camera by %.3f tiles after 1 s of clear weather, %.3f after 1 h, %.3f after 3 h\n",
                tiles[0], tiles[1], tiles[2], tiles[3], afterASecond, afterAnHour, afterThreeHours);
    Check(tiles[0] != tiles[2] || tiles[1] != tiles[3], "Hyjal's clear and storm haze noise differ in tile size");
    Check(afterASecond < kMaxBlendPathTiles && std::fabs(afterAnHour - afterASecond) < kPathGrowthTolerance &&
              std::fabs(afterThreeHours - afterASecond) < kPathGrowthTolerance,
          "a weather blend between noise tiles moves the pattern near the camera by a bounded amount that does not "
          "grow with the time the noise has scrolled");
}

using VolumeTexel = BYTE (*)(UINT, UINT, UINT);

struct NoiseVolumeShape
{
    UINT size[3];
    VolumeTexel texel;
};

const NoiseVolumeShape kAuthoredVolume = {{kAuthoredNoiseSize, kAuthoredNoiseSize, kAuthoredNoiseSize},
                                          AuthoredNoiseTexel};

double TrilinearSample(const NoiseVolumeShape& volume, const double* tileCoordinate)
{
    int base[3];
    double fraction[3];
    for (int axis = 0; axis < 3; ++axis)
    {
        const double texel = tileCoordinate[axis] * volume.size[axis] - 0.5;
        const double floored = std::floor(texel);
        base[axis] = static_cast<int>(floored);
        fraction[axis] = texel - floored;
    }
    double value = 0.0;
    for (int corner = 0; corner < 8; ++corner)
    {
        double weight = 1.0;
        UINT texel[3];
        for (int axis = 0; axis < 3; ++axis)
        {
            const int step = (corner >> axis) & 1;
            const int size = static_cast<int>(volume.size[axis]);
            weight *= step ? fraction[axis] : 1.0 - fraction[axis];
            texel[axis] = static_cast<UINT>((base[axis] + step + size) % size);
        }
        value += weight * volume.texel(texel[0], texel[1], texel[2]) / 255.0;
    }
    return value;
}

double CpuNoiseSample(const float (&registers)[4][4], const double* position,
                      const NoiseVolumeShape& volume = kAuthoredVolume)
{
    double sample = 0.0;
    for (int octave = 0; octave < kAuthoredNoiseOctaves; ++octave)
    {
        double tileCoordinate[3];
        for (int axis = 0; axis < 3; ++axis)
            tileCoordinate[axis] = PositiveFraction((position[axis] - registers[octave][axis]) * registers[octave][3]);
        sample += registers[3][octave] * TrilinearSample(volume, tileCoordinate);
    }
    return sample;
}

struct ProbeTargets
{
    IDirect3DSurface9* target = nullptr;
    IDirect3DSurface9* readback = nullptr;

    ~ProbeTargets()
    {
        if (target)
            target->Release();
        if (readback)
            readback->Release();
    }
};

bool RenderNoiseProbe(IDirect3DDevice9* device, const ProbeTargets& targets, const float* origin, const float* step,
                      const float (&registers)[4][4], std::vector<float>& rgba)
{
    const float quad[4][4] = {{-0.5f, -0.5f, 0, 1}, {kProbeSide - 0.5f, -0.5f, 0, 1},
                             {-0.5f, kProbeSide - 0.5f, 0, 1}, {kProbeSide - 0.5f, kProbeSide - 0.5f, 0, 1}};
    const float noVariation[4] = {};
    device->SetPixelShaderConstantF(0, origin, 1);
    device->SetPixelShaderConstantF(1, step, 1);
    device->SetPixelShaderConstantF(36, &registers[0][0], 4);
    device->SetPixelShaderConstantF(78, noVariation, 1);
    if (FAILED(device->BeginScene()))
        return false;
    const HRESULT draw = device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0]));
    device->EndScene();
    D3DLOCKED_RECT locked = {};
    if (FAILED(draw) || FAILED(device->GetRenderTargetData(targets.target, targets.readback)) ||
        FAILED(targets.readback->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
        return false;
    rgba.resize(kProbeSide * kProbeSide * 4);
    for (UINT y = 0; y < kProbeSide; ++y)
        std::memcpy(&rgba[y * kProbeSide * 4], static_cast<const BYTE*>(locked.pBits) + y * locked.Pitch,
                    kProbeSide * 4 * sizeof(float));
    targets.readback->UnlockRect();
    return true;
}

struct ProbeCase
{
    float origin[4];
    float step[4];
    float registers[4][4];
};

void CheckProbeMatchesCpuNoise(IDirect3DDevice9* device)
{
    ProbeTargets targets;
    IDirect3DPixelShader9* shader = nullptr;
    IDirect3DVolumeTexture9* volume = nullptr;
    IDirect3DStateBlock9* previous = nullptr;
    IDirect3DSurface9* previousTarget = nullptr;
    const bool ready =
        SUCCEEDED(device->CreateStateBlock(D3DSBT_ALL, &previous)) &&
        SUCCEEDED(device->GetRenderTarget(0, &previousTarget)) &&
        SUCCEEDED(device->CreateRenderTarget(kProbeSide, kProbeSide, D3DFMT_A32B32G32R32F, D3DMULTISAMPLE_NONE, 0,
                                             FALSE, &targets.target, nullptr)) &&
        SUCCEEDED(device->CreateOffscreenPlainSurface(kProbeSide, kProbeSide, D3DFMT_A32B32G32R32F, D3DPOOL_SYSTEMMEM,
                                                      &targets.readback, nullptr)) &&
        SUCCEEDED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(g_ps_density_probe), &shader)) &&
        CreateAuthoredNoise(device, &volume);
    Check(ready, "authored noise probe, float target and noise volume created");
    if (ready)
    {
        const D3DVIEWPORT9 viewport = {0, 0, kProbeSide, kProbeSide, 0.0f, 1.0f};
        device->SetRenderTarget(0, targets.target);
        device->SetViewport(&viewport);
        device->SetVertexShader(nullptr);
        device->SetFVF(D3DFVF_XYZRHW);
        device->SetPixelShader(shader);
        device->SetTexture(10, volume);
        device->SetSamplerState(10, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        device->SetSamplerState(10, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        device->SetSamplerState(10, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        for (D3DSAMPLERSTATETYPE address : {D3DSAMP_ADDRESSU, D3DSAMP_ADDRESSV, D3DSAMP_ADDRESSW})
            device->SetSamplerState(10, address, D3DTADDRESS_WRAP);
        device->SetSamplerState(10, D3DSAMP_SRGBTEXTURE, FALSE);
        device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
        const ProbeCase cases[] = {
            {{-9100.0f, -100.0f, 80.0f, 0}, {7.0f, 11.0f, 0, 0},
             {{13.2f, -7.5f, 3.1f, 1.0f / 300.0f}, {201.0f, 5.0f, 99.0f, 1.0f / 250.0f}, {0.2f, 0.3f, 0.4f, 1.0f},
              {0.5f, 0.5f, 0, 0}}},
            {{5458.0f, -2934.0f, 1481.0f, 0}, {173.0f, 97.0f, 0, 0},
             {{4321.5f, 1234.25f, 999.9f, 1.0f / 5000.0f}, {0, 0, 0, 1.0f / 5000.0f}, {0, 0, 0, 0.5f},
              {1.0f, 0.0f, 0, 0}}},
            {{-10500.0f, 2500.0f, 50.0f, 0}, {3.0f, 5.0f, 0, 0},
             {{150.0f, 150.0f, 0.0f, 1.0f / 300.0f}, {299.5f, 0.25f, 0.0f, 1.0f / 300.0f}, {0, 0, 0, 1.0f},
              {0.25f, 0.75f, 0, 0}}},
        };
        float worstSample = 0.0f;
        float worstCurve = 0.0f;
        bool rendered = true;
        for (const ProbeCase& c : cases)
        {
            std::vector<float> rgba;
            rendered = RenderNoiseProbe(device, targets, c.origin, c.step, c.registers, rgba) && rendered;
            if (!rendered)
                break;
            for (UINT y = 0; y < kProbeSide; ++y)
                for (UINT x = 0; x < kProbeSide; ++x)
                {
                    const double position[3] = {c.origin[0] + x * c.step[0], c.origin[1] + y * c.step[1],
                                                c.origin[2]};
                    const float* texel = &rgba[(y * kProbeSide + x) * 4];
                    const float gpuSample = texel[2];
                    const float alpha = c.registers[2][3];
                    const float curve = 1.0f + (NoiseDensityCurve(gpuSample) - 1.0f) * alpha;
                    worstSample = std::fmax(worstSample,
                                            std::fabs(gpuSample - static_cast<float>(CpuNoiseSample(c.registers,
                                                                                                    position))));
                    worstCurve = std::fmax(worstCurve, std::fabs(texel[1] - curve));
                }
        }
        std::printf("     authored noise probe: largest sample difference from the CPU volume %.4f, largest density "
                    "difference from the CPU curve %.2g\n",
                    worstSample, worstCurve);
        Check(rendered && worstSample <= kProbeSampleTolerance,
              "the shader samples the noise volume at frac((p - offset) / tile) for both octaves, as the CPU does");
        Check(rendered && worstCurve <= kProbeCurveTolerance,
              "the shader's noise density is lerp(1, curve(sample), alpha) with the CPU's curve");
    }
    if (previousTarget)
    {
        device->SetRenderTarget(0, previousTarget);
        previousTarget->Release();
    }
    if (previous)
    {
        previous->Apply();
        previous->Release();
    }
    if (shader)
        shader->Release();
    if (volume)
        volume->Release();
}

bool CreateConstantVolume(IDirect3DDevice9* device, BYTE value, IDirect3DVolumeTexture9** volume)
{
    if (FAILED(device->CreateVolumeTexture(4, 4, 4, 1, 0, D3DFMT_L8, D3DPOOL_MANAGED, volume, nullptr)))
        return false;
    D3DLOCKED_BOX locked = {};
    if (FAILED((*volume)->LockBox(0, &locked, nullptr, 0)))
        return false;
    for (UINT z = 0; z < 4; ++z)
        for (UINT y = 0; y < 4; ++y)
            std::memset(static_cast<BYTE*>(locked.pBits) + z * locked.SlicePitch + y * locked.RowPitch, value, 4);
    return SUCCEEDED((*volume)->UnlockBox(0));
}

struct MarchedPixel
{
    float radiance;
    float opacity;
};

MarchedPixel ReadMarchedPixel(IDirect3DDevice9* device, const FogIntegrationResources& resources)
{
    MarchedPixel pixel = {-1.0f, -1.0f};
    D3DLOCKED_RECT locked = {};
    if (FAILED(device->GetRenderTargetData(resources.target, resources.readback)) ||
        FAILED(resources.readback->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
        return pixel;
    const BYTE* bgra = static_cast<const BYTE*>(locked.pBits) + 4 * locked.Pitch + 4 * 4;
    pixel = {bgra[2] / 255.0f, bgra[3] / 255.0f};
    resources.readback->UnlockRect();
    return pixel;
}

MarchedPixel MarchNoisyLayer(IDirect3DDevice9* device, const FogIntegrationResources& resources, float alpha,
                             const float* noiseRegisters, const float* camera = kWorldOrigin)
{
    float constants[99][4] = {};
    constants[0][2] = constants[0][3] = 8.0f;
    constants[1][0] = 1.0f;
    constants[1][2] = constants[1][3] = 1.0f;
    constants[2][0] = constants[2][1] = 1.0f;
    constants[3][0] = 1.0004f;
    constants[3][1] = -0.40016f;
    constants[3][2] = kMarchLength;
    constants[3][3] = 0.94f;
    for (int row = 0; row < 4; ++row)
        constants[4 + row][row] = 1.0f;
    std::memcpy(constants[7], camera, 3 * sizeof(float));
    constants[8][0] = constants[8][1] = 8.0f;
    constants[8][2] = constants[8][3] = 0.125f;
    constants[9][2] = constants[9][3] = 1.0f;
    constants[11][1] = constants[11][3] = kMarchLength;
    constants[11][2] = 850.0f;
    constants[12][1] = kLayerDensity;
    constants[14][3] = 1.0f;
    constants[16][3] = 1.0f;
    constants[17][2] = kLayerLength;
    std::memcpy(constants[36], noiseRegisters, 4 * sizeof(constants[36]));
    constants[38][3] = alpha;
    device->SetPixelShaderConstantF(0, &constants[0][0], 99);
    const float quad[4][4] = {{-0.5f, -0.5f, 0, 1}, {7.5f, -0.5f, 0, 1}, {-0.5f, 7.5f, 0, 1}, {7.5f, 7.5f, 0, 1}};
    device->BeginScene();
    device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0]));
    device->EndScene();
    return ReadMarchedPixel(device, resources);
}

void PrepareMarchFixture(IDirect3DDevice9* device, const FogIntegrationResources& resources,
                         D3DTEXTUREFILTERTYPE noiseFilter)
{
    const D3DVIEWPORT9 viewport = {0, 0, 8, 8, 0.0f, 1.0f};
    device->SetDepthStencilSurface(nullptr);
    device->SetRenderTarget(0, resources.target);
    device->SetViewport(&viewport);
    device->SetVertexShader(nullptr);
    device->SetFVF(D3DFVF_XYZRHW);
    device->SetTexture(0, resources.depth);
    for (DWORD stage : {0ul, 10ul})
    {
        const D3DTEXTUREFILTERTYPE filter = stage == 10ul ? noiseFilter : D3DTEXF_POINT;
        device->SetSamplerState(stage, D3DSAMP_MINFILTER, filter);
        device->SetSamplerState(stage, D3DSAMP_MAGFILTER, filter);
        device->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        device->SetSamplerState(stage, D3DSAMP_SRGBTEXTURE, FALSE);
    }
    for (D3DSAMPLERSTATETYPE address : {D3DSAMP_ADDRESSU, D3DSAMP_ADDRESSV, D3DSAMP_ADDRESSW})
        device->SetSamplerState(10, address, D3DTADDRESS_WRAP);
    device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
}

void RestoreMarchFixture(IDirect3DDevice9* device, const FogIntegrationResources& resources)
{
    device->SetTexture(10, nullptr);
    device->SetRenderTarget(0, resources.previousTarget);
    device->SetDepthStencilSurface(resources.previousDepth);
    if (resources.previousState)
        resources.previousState->Apply();
}

const BYTE* const kNoisyMarches[6] = {g_ps_noisy_march_low,     g_ps_noisy_march_mid,     g_ps_noisy_march_high,
                                      g_ps_lit_noisy_march_low, g_ps_lit_noisy_march_mid, g_ps_lit_noisy_march_high};
const BYTE* const kPlainMarches[6] = {g_ps_march_low,     g_ps_march_mid,     g_ps_march_high,
                                      g_ps_lit_march_low, g_ps_lit_march_mid, g_ps_lit_march_high};
constexpr int kMarchSteps[6] = {16, 24, 32, 16, 24, 32};

void CheckMarchAppliesNoise(IDirect3DDevice9* device)
{
    FogIntegrationResources resources;
    IDirect3DVolumeTexture9* volumes[2] = {};
    const BYTE values[2] = {102, 153};
    const bool ready = CreateFogIntegrationResources(device, resources) &&
                       CreateConstantVolume(device, values[0], &volumes[0]) &&
                       CreateConstantVolume(device, values[1], &volumes[1]);
    Check(ready, "constant noise volumes and march targets created");
    if (ready)
    {
        PrepareMarchFixture(device, resources, D3DTEXF_POINT);
        const float noise[4][4] = {{12.0f, -3.0f, 7.0f, 1.0f / kNoiselessTile},
                                   {-40.0f, 9.0f, 0.0f, 1.0f / kNoiselessTile},
                                   {1.0f, 1.0f, 1.0f, 0.0f},
                                   {0.5f, 0.5f, 0.0f, 0.0f}};
        const float alphas[] = {1.0f, 0.5f};
        float worst = 0.0f;
        float worstSilent = 0.0f;
        bool created = true;
        for (int variant = 0; variant < 6; ++variant)
        {
            IDirect3DPixelShader9* noisy = nullptr;
            IDirect3DPixelShader9* plain = nullptr;
            created = SUCCEEDED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(kNoisyMarches[variant]),
                                                          &noisy)) &&
                      SUCCEEDED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(kPlainMarches[variant]),
                                                          &plain)) &&
                      created;
            for (int v = 0; noisy && plain && v < 2; ++v)
            {
                device->SetTexture(10, volumes[v]);
                device->SetPixelShader(noisy);
                const float curve = NoiseDensityCurve(values[v] / 255.0f);
                for (float alpha : alphas)
                {
                    const float density = 1.0f + (curve - 1.0f) * alpha;
                    const float opacity = 1.0f - std::exp(-kLayerDensity * kLayerLength * density);
                    const MarchedPixel pixel = MarchNoisyLayer(device, resources, alpha, &noise[0][0]);
                    worst = std::fmax(worst, std::fabs(pixel.opacity - opacity));
                    worst = std::fmax(worst, std::fabs(pixel.radiance - (1.0f - density) * opacity));
                }
                const MarchedPixel silent = MarchNoisyLayer(device, resources, 0.0f, &noise[0][0]);
                device->SetPixelShader(plain);
                const MarchedPixel reference = MarchNoisyLayer(device, resources, 0.0f, &noise[0][0]);
                worstSilent = std::fmax(worstSilent, std::fmax(std::fabs(silent.radiance - reference.radiance),
                                                               std::fabs(silent.opacity - reference.opacity)));
            }
            if (noisy)
                noisy->Release();
            if (plain)
                plain->Release();
        }
        std::printf("     noisy march with constant noise 0.4 and 0.6 at alpha 1 and 0.5: largest error %.2f/255; at "
                    "alpha 0 it differs from the noise-free march by %.2f/255\n",
                    worst * 255.0f, worstSilent * 255.0f);
        Check(created && worst <= kMarchTolerance,
              "every noisy march variant thins the layer by lerp(1, curve(noise), alpha) and fades its emission");
        Check(created && worstSilent <= kSilentNoiseTolerance,
              "a noisy march at alpha 0 matches the noise-free march the renderer uses without noise");
    }
    RestoreMarchFixture(device, resources);
    for (IDirect3DVolumeTexture9* volume : volumes)
        if (volume)
            volume->Release();
}

BYTE RampTexel(UINT, UINT, UINT z)
{
    return static_cast<BYTE>(std::lround(kRampMean + kRampAmplitude * std::sin(kTwoPi * (z + 0.5) / kRampDepth)));
}

const NoiseVolumeShape kRampVolume = {{kRampSide, kRampSide, kRampDepth}, RampTexel};

bool CreateRampVolume(IDirect3DDevice9* device, IDirect3DVolumeTexture9** volume)
{
    if (FAILED(device->CreateVolumeTexture(kRampSide, kRampSide, kRampDepth, 1, 0, D3DFMT_L8, D3DPOOL_MANAGED, volume,
                                           nullptr)))
        return false;
    D3DLOCKED_BOX locked = {};
    if (FAILED((*volume)->LockBox(0, &locked, nullptr, 0)))
        return false;
    for (UINT z = 0; z < kRampDepth; ++z)
        for (UINT y = 0; y < kRampSide; ++y)
            std::memset(static_cast<BYTE*>(locked.pBits) + z * locked.SlicePitch + y * locked.RowPitch,
                        RampTexel(0, y, z), kRampSide);
    return SUCCEEDED((*volume)->UnlockBox(0));
}

MarchedPixel CpuNoisyMarch(int steps, const float* camera, const float (&registers)[4][4])
{
    const double rayLength = std::sqrt(2.0 * kProbedPixelRay * kProbedPixelRay + 1.0);
    const double direction[3] = {kProbedPixelRay / rayLength, -kProbedPixelRay / rayLength, 1.0 / rayLength};
    const double squaredSteps = static_cast<double>(steps) * steps;
    double radiance = 0.0;
    double transmittance = 1.0;
    for (int step = 0; step < steps; ++step)
    {
        const double start = kMarchLength * step * step / squaredSteps;
        const double end = kMarchLength * (step + 1) * (step + 1) / squaredSteps;
        if (start >= kLayerLength)
            break;
        const double middle = start + (end - start) * 0.5;
        const double position[3] = {camera[0] + direction[0] * middle, camera[1] + direction[1] * middle,
                                    camera[2] + direction[2] * middle};
        const double sample = std::clamp(CpuNoiseSample(registers, position, kRampVolume), 0.0, 1.0);
        const double density = 1.0 + (NoiseDensityCurve(static_cast<float>(sample)) - 1.0) * registers[2][3];
        const double inLayer = std::min(end, static_cast<double>(kLayerLength)) - start;
        const double opticalDepth = kLayerDensity * inLayer * density;
        if (opticalDepth <= 0.0)
            continue;
        const double opacity = 1.0 - std::exp(-opticalDepth);
        radiance += transmittance * (1.0 - density) * opacity;
        transmittance *= 1.0 - opacity;
    }
    return {static_cast<float>(radiance), static_cast<float>(1.0 - transmittance)};
}

void CheckMarchSamplesNoiseAlongTheWorldRay(IDirect3DDevice9* device)
{
    FogIntegrationResources resources;
    IDirect3DVolumeTexture9* ramp = nullptr;
    const bool ready = CreateFogIntegrationResources(device, resources) && CreateRampVolume(device, &ramp);
    Check(ready, "ramp noise volume and march targets created");
    if (ready)
    {
        PrepareMarchFixture(device, resources, D3DTEXF_LINEAR);
        device->SetTexture(10, ramp);
        const float registers[4][4] = {{13.0f, -7.0f, 55.0f, 1.0f / 400.0f},
                                       {-20.0f, 31.0f, -20.0f, 1.0f / 250.0f},
                                       {1.0f, 1.0f, 1.0f, 1.0f},
                                       {0.5f, 0.5f, 0.0f, 0.0f}};
        const float* cameras[2] = {kWorldOrigin, kShiftedCamera};
        float worst = 0.0f;
        float weakestShift = 1.0f;
        bool created = true;
        for (int variant = 0; variant < 6; ++variant)
        {
            IDirect3DPixelShader9* noisy = nullptr;
            created = SUCCEEDED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(kNoisyMarches[variant]),
                                                          &noisy)) &&
                      created;
            if (!noisy)
                continue;
            device->SetPixelShader(noisy);
            MarchedPixel marched[2] = {};
            for (int c = 0; c < 2; ++c)
            {
                marched[c] = MarchNoisyLayer(device, resources, registers[2][3], &registers[0][0], cameras[c]);
                const MarchedPixel expected = CpuNoisyMarch(kMarchSteps[variant], cameras[c], registers);
                worst = std::fmax(worst, std::fmax(std::fabs(marched[c].radiance - expected.radiance),
                                                   std::fabs(marched[c].opacity - expected.opacity)));
            }
            weakestShift = std::fmin(weakestShift, std::fmax(std::fabs(marched[0].radiance - marched[1].radiance),
                                                             std::fabs(marched[0].opacity - marched[1].opacity)));
            noisy->Release();
        }
        std::printf("     noisy march through a ramp volume: largest error from the CPU march at the world sample "
                    "points %.2f/255; moving the camera changes every variant by at least %.1f/255\n",
                    worst * 255.0f, weakestShift * 255.0f);
        Check(created && worst <= kMarchTolerance && weakestShift >= kCameraShiftSensitivity,
              "every noisy march variant samples the noise at camera + direction x distance of each step's sample "
              "point, in world space, for both octaves");
    }
    RestoreMarchFixture(device, resources);
    if (ramp)
        ramp->Release();
}

void CheckNoisyCompositeThinsSilhouettes(IDirect3DDevice9* device)
{
    silhouette_quality::Fixture fixture(device);
    IDirect3DVolumeTexture9* volume = nullptr;
    const bool ready = fixture.Create(silhouette_quality::kNoisyComposites) &&
                       CreateConstantVolume(device, kThinningNoise, &volume);
    fixture.layerNoiseVolume = volume;
    Check(ready, "noisy composite silhouette fixture created");
    if (!ready)
        return;
    const float layerNoise[4][4] = {{0.0f, 0.0f, 0.0f, 1.0f / kNoiselessTile},
                                    {0.0f, 0.0f, 0.0f, 1.0f / kNoiselessTile},
                                    {1.0f, 1.0f, 1.0f, 1.0f},
                                    {0.5f, 0.5f, 0.0f, 0.0f}};
    std::memcpy(fixture.layerNoise, layerNoise, sizeof(layerNoise));
    fixture.densityScale = NoiseDensityCurve(kThinningNoise / 255.0f);
    float worst = 0.0f;
    const bool drawn = silhouette_quality::WorstStripError(fixture, worst);
    std::printf("     noisy composite at thin silhouettes with noise %.2f (density x%.3f): largest transmittance "
                "error %.2f/255\n",
                kThinningNoise / 255.0f, fixture.densityScale, worst * 255.0f);
    Check(drawn && worst <= kMarchTolerance,
          "every noisy composite marches thin silhouettes at full resolution through the authored noise");
}

template <size_t Size>
bool ShaderRuns(IDirect3DPixelShader9* shader, const BYTE (&bytecode)[Size])
{
    UINT size = 0;
    if (!shader || FAILED(shader->GetFunction(nullptr, &size)) || size != Size)
        return false;
    std::vector<BYTE> function(size);
    return SUCCEEDED(shader->GetFunction(function.data(), &size)) && std::memcmp(function.data(), bytecode, Size) == 0;
}

struct DrawnShaders
{
    IDirect3DPixelShader9* march = nullptr;
    IDirect3DPixelShader9* composite = nullptr;
    IDirect3DPixelShader9* splitComposite = nullptr;
};

void CheckRendererDrawsStormNoise(Harness& harness)
{
    Config saved;
    vf_test_get_config(&saved);
    Config config = saved;
    config.quality = 2;
    config.noiseAmount = 0.0f;
    config.temporal = 0.0f;
    config.godRays = 0.0f;
    config.dataMode = 1;
    config.localLights = false;
    const D3DVIEWPORT9 world = {0, 0, 128, 96, 0, 1};
    const Vec3 at = Add(kHarbourEye, {100, 100, -5});
    float view[16];
    float projection[16];
    CameraRelativeLookAt(kHarbourEye, at, view);
    EngineProjection(128.0f / 96.0f, projection);
    FrameInputs input = MakeInputs(view, projection, kHarbourEye, at, world);
    input.mapId = kEasternKingdoms;
    input.dayFraction = kNoon;
    input.lightParams = Storm(kFullStorm);
    Image frames[2];
    bool adaptive[2] = {};
    DrawnShaders drawn[2];
    bool rendered = true;
    bool statesKept = true;
    for (int noisy = 0; noisy < 2; ++noisy)
    {
        config.classicNoise = noisy != 0;
        vf_test_set_config(&config);
        harness.BeginFrame();
        harness.DrawScene(kHarbourEye, view, projection, world);
        Sentinel before;
        ReadSentinel(harness.dev, before);
        const char* skip = "";
        rendered = vf_test_render(&input, &skip) != 0 && rendered;
        Sentinel after;
        ReadSentinel(harness.dev, after);
        statesKept = statesKept && SameSentinel(before, after);
        ReleaseSentinel(before);
        ReleaseSentinel(after);
        adaptive[noisy] = vf_test_adaptive_lighting_history() != 0;
        vf_test_drawn_fog_shaders(&drawn[noisy].march, &drawn[noisy].composite, &drawn[noisy].splitComposite);
        frames[noisy] = Capture(harness.dev);
        harness.dev->EndScene();
        harness.dev->Present(nullptr, nullptr, nullptr, nullptr);
    }
    const double change = MeanLumaChange(frames[0], frames[1], 0, 0, world.Width, world.Height);
    std::printf("     harbour storm at noon: authored noise changes the mean luma by %.4f\n", change);
    Check(rendered && change > 0.002, "the renderer draws the storm's authored noise");
    Check(rendered && adaptive[1] && !adaptive[0],
          "authored noise makes the temporal filter treat the fog as animated");
    Check(rendered && statesKept, "drawing authored noise restores every state, the noise sampler included");
    Check(rendered && ShaderRuns(drawn[1].march, g_ps_noisy_march_mid) &&
              ShaderRuns(drawn[1].composite, g_ps_noisy_composite_mid) &&
              ShaderRuns(drawn[0].march, g_ps_march_mid) && ShaderRuns(drawn[0].composite, g_ps_composite_mid),
          "a storm with authored noise draws the noisy march and the noisy composite, whose silhouettes sample the "
          "noise; without noise the renderer keeps the noise-free shaders");
    vf_test_set_config(&saved);
}

void CheckAuthoredNoise(const FogData& data)
{
    CheckCurveMatchesForever();
    CheckVolumeIsBalancedAndTileable();
    CheckModelTakesAuthoredNoise(data);
    CheckScrollDriftsWithinOneTile();
    CheckStormKeepsTheScrollAcrossTileBlends(data);
}

void CheckAuthoredNoiseOnTheGpu(IDirect3DDevice9* device)
{
    CheckProbeMatchesCpuNoise(device);
    CheckMarchAppliesNoise(device);
    CheckMarchSamplesNoiseAlongTheWorldRay(device);
    CheckNoisyCompositeThinsSilhouettes(device);
}
}
