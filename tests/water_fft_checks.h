#pragma once

namespace water_fft_checks
{
constexpr int kReferenceResolution = 256;
constexpr float kWindSpeed = 2.0f;
constexpr double kSimulatedSeconds = 12.345;
constexpr double kStatisticsSeconds = 10.0;
constexpr float kFrameSeconds = 0.05f;
constexpr float kLongestFoamStepSeconds = 0.1f;
constexpr double kExactTurn = 6.283185307179586;
constexpr double kDoubleTolerance = 1.0e-10;
constexpr double kGoldenTolerance = 1.0e-9;
constexpr double kFloatTolerancePerStage = 1.0e-5;
constexpr double kFloatTolerance = 1.0e-5;
constexpr double kParsevalTolerance = 1.0e-4;
constexpr double kHalfRelativeTolerance = 2.0e-3;
constexpr double kHalfAbsoluteTolerance = 1.0e-3;
constexpr double kChainEndTolerance = 5.0e-3;
constexpr double kSmallestHalfStep = 5.9604644775390625e-8;
constexpr double kFoamStateTolerance = 1.0e-5;
constexpr double kKitStatisticsTolerance = 0.15;
constexpr double kNoiseMeanTolerance = 0.02;
constexpr double kNoiseVarianceTolerance = 0.05;
constexpr double kEquilibriumTolerance = 1.0e-6;
constexpr double kSwampFoamBias = 0.092;
constexpr double kSwampFoamRate = 12.0;
constexpr double kSwampFoamDecay = 1.0;
constexpr double kSwampFoamEquilibrium =
    kSwampFoamRate * kSwampFoamBias / (kSwampFoamRate * kSwampFoamBias + kSwampFoamDecay);
constexpr int kEquilibriumFrames = 300;
constexpr int kWarmupFrames = 8;
constexpr int kTimedFrames = 32;
constexpr double kTimedFrameSeconds = 1.0 / 60.0;
constexpr ULONGLONG kQueryTimeoutMs = 10000;
constexpr ULONGLONG kGpuClockWarmupMs = 1500;
constexpr int kSingleModeResolution = 128;
constexpr float kSingleModeLength = 64.0f;
constexpr int kSingleModeCycles = 3;
constexpr double kSingleModeAmplitude = 0.5;
constexpr float kCrestFoamStepSeconds = 0.1f;
constexpr UINT kVertexFloatConstants = 256;
constexpr UINT kPixelFloatConstants = 224;
constexpr UINT kFirstCallerPixelConstant = 16;
constexpr UINT kIntConstants = 16;
constexpr UINT kBoolConstants = 16;
constexpr DWORD kPixelSamplers = 16;
constexpr DWORD kSentinelTextureStage = 5;
constexpr DWORD kAllChannels = 0xF;
constexpr int kExtraTargets = 3;
constexpr uint32_t kRandomSeed = 20260929u;

const WaterFftTile kLakeTiles[] = {
    {20, 32.0f, 7689.0f, 3.0f, 4.0f, {-1.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 1.0f}},
    {24, 64.0f, 50000.0f, 3.0f, 5.0f, {-0.16f, 12.0f, 3.0f}, {-0.16f, 2.0f, 2.0f}},
    {122, 16.0f, 30000.0f, 1.8f, 4.0f, {-1.0f, 0.0f, 1.0f}, {-0.158f, 1.0f, 0.506f}},
};

const WaterFftTile kOceanTiles[] = {
    {44, 128.0f, 42000.0f, 1.8f, 2.0f, {-1.0f, 0.0f, 1.0f}, {-0.163f, 0.445f, 0.3f}},
    {45, 256.0f, 36000.0f, 1.8f, 5.0f, {-0.29f, 12.0f, 0.9f}, {-0.36f, 100.0f, 0.7f}},
    {46, 512.0f, 16000.0f, 1.8f, 6.0f, {-0.2f, 20.0f, 1.5f}, {-0.35f, 200.0f, 0.7f}},
    {96, 32.0f, 200000.0f, 4.0f, 6.0f, {-1.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 1.0f}},
};

const WaterFftTile kStillSwampTile = {139, 64.0f, 0.0f, 1.8f, 4.0f, {0.092f, 12.0f, 1.0f}, {-1.0f, 0.0f, 1.0f}};
const WaterFftTile kStillLakeTile = {24, 64.0f, 0.0f, 3.0f, 5.0f, {-0.16f, 12.0f, 3.0f}, {-1.0f, 0.0f, 1.0f}};
const WaterFftTile kCrestFoamTile = {0, kSingleModeLength, 0.0f, 1.0f, 1.0f, {0.0f, 1.0f, 0.0f}, {-1.0f, 0.0f, 1.0f}};

struct KitHeightRms
{
    uint32_t tile;
    double heightRms;
};

const KitHeightRms kKitHeightRmsAt256[] = {{20, 0.020}, {24, 0.111}, {122, 0.008}, {44, 0.075},
                                           {45, 0.112}, {46, 0.132}, {96, 0.114}};

struct GoldenTexel
{
    int x;
    int y;
    double h0[4];
};

const GoldenTexel kTile24GoldenAt256[] = {
    {131, 129, {40.474028812279563, 1185.2154946030441, 0.0, 0.0}},
    {140, 120, {36.618780712626027, 67.944560393117769, 0.0, 0.0}},
    {133, 128, {-301.60764242153147, 737.54214150239966, 0.0, 0.0}},
    {125, 127, {0.0, 0.0, 324.32948531556923, 698.24140829935141}},
    {100, 140, {0.0, 0.0, 8.0561458173942011, 0.70367900772148251}},
    {200, 130, {3.559967980566626, 0.020079326391551734, 0.0, 0.0}},
};

struct RenderStateValue
{
    D3DRENDERSTATETYPE state;
    DWORD value;
};

const RenderStateValue kCallerRenderStates[] = {
    {D3DRS_ZENABLE, D3DZB_FALSE},        {D3DRS_ZWRITEENABLE, FALSE},       {D3DRS_ALPHATESTENABLE, FALSE},
    {D3DRS_ALPHABLENDENABLE, FALSE},     {D3DRS_STENCILENABLE, FALSE},      {D3DRS_SCISSORTESTENABLE, FALSE},
    {D3DRS_CLIPPLANEENABLE, 0},          {D3DRS_FOGENABLE, FALSE},          {D3DRS_SRGBWRITEENABLE, FALSE},
    {D3DRS_COLORWRITEENABLE, kAllChannels}, {D3DRS_CULLMODE, D3DCULL_NONE}, {D3DRS_FILLMODE, D3DFILL_SOLID},
};

const RenderStateValue kSentinelRenderStates[] = {
    {D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR}, {D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR}, {D3DRS_BLENDOP, D3DBLENDOP_MAX},
    {D3DRS_STENCILREF, 0x5A},             {D3DRS_TEXTUREFACTOR, 0x12345678},     {D3DRS_BLENDFACTOR, 0xFFAA5500},
    {D3DRS_COLORWRITEENABLE2, 0x3},       {D3DRS_ALPHAREF, 0x40},                {D3DRS_AMBIENT, 0xFF102030},
};

const D3DRENDERSTATETYPE kEveryRenderState[] = {
    D3DRS_ZENABLE, D3DRS_FILLMODE, D3DRS_SHADEMODE, D3DRS_ZWRITEENABLE, D3DRS_ALPHATESTENABLE, D3DRS_LASTPIXEL,
    D3DRS_SRCBLEND, D3DRS_DESTBLEND, D3DRS_CULLMODE, D3DRS_ZFUNC, D3DRS_ALPHAREF, D3DRS_ALPHAFUNC,
    D3DRS_DITHERENABLE, D3DRS_ALPHABLENDENABLE, D3DRS_FOGENABLE, D3DRS_SPECULARENABLE, D3DRS_FOGCOLOR,
    D3DRS_FOGTABLEMODE, D3DRS_FOGSTART, D3DRS_FOGEND, D3DRS_FOGDENSITY, D3DRS_RANGEFOGENABLE, D3DRS_STENCILENABLE,
    D3DRS_STENCILFAIL, D3DRS_STENCILZFAIL, D3DRS_STENCILPASS, D3DRS_STENCILFUNC, D3DRS_STENCILREF,
    D3DRS_STENCILMASK, D3DRS_STENCILWRITEMASK, D3DRS_TEXTUREFACTOR, D3DRS_WRAP0, D3DRS_WRAP1, D3DRS_WRAP2,
    D3DRS_WRAP3, D3DRS_WRAP4, D3DRS_WRAP5, D3DRS_WRAP6, D3DRS_WRAP7, D3DRS_CLIPPING, D3DRS_LIGHTING,
    D3DRS_AMBIENT, D3DRS_FOGVERTEXMODE, D3DRS_COLORVERTEX, D3DRS_LOCALVIEWER, D3DRS_NORMALIZENORMALS,
    D3DRS_DIFFUSEMATERIALSOURCE, D3DRS_SPECULARMATERIALSOURCE, D3DRS_AMBIENTMATERIALSOURCE,
    D3DRS_EMISSIVEMATERIALSOURCE, D3DRS_VERTEXBLEND, D3DRS_CLIPPLANEENABLE, D3DRS_POINTSIZE, D3DRS_POINTSIZE_MIN,
    D3DRS_POINTSPRITEENABLE, D3DRS_POINTSCALEENABLE, D3DRS_POINTSCALE_A, D3DRS_POINTSCALE_B, D3DRS_POINTSCALE_C,
    D3DRS_MULTISAMPLEANTIALIAS, D3DRS_MULTISAMPLEMASK, D3DRS_PATCHEDGESTYLE, D3DRS_DEBUGMONITORTOKEN,
    D3DRS_POINTSIZE_MAX, D3DRS_INDEXEDVERTEXBLENDENABLE, D3DRS_COLORWRITEENABLE, D3DRS_TWEENFACTOR, D3DRS_BLENDOP,
    D3DRS_POSITIONDEGREE, D3DRS_NORMALDEGREE, D3DRS_SCISSORTESTENABLE, D3DRS_SLOPESCALEDEPTHBIAS,
    D3DRS_ANTIALIASEDLINEENABLE, D3DRS_MINTESSELLATIONLEVEL, D3DRS_MAXTESSELLATIONLEVEL, D3DRS_ADAPTIVETESS_X,
    D3DRS_ADAPTIVETESS_Y, D3DRS_ADAPTIVETESS_Z, D3DRS_ADAPTIVETESS_W, D3DRS_ENABLEADAPTIVETESSELLATION,
    D3DRS_TWOSIDEDSTENCILMODE, D3DRS_CCW_STENCILFAIL, D3DRS_CCW_STENCILZFAIL, D3DRS_CCW_STENCILPASS,
    D3DRS_CCW_STENCILFUNC, D3DRS_COLORWRITEENABLE1, D3DRS_COLORWRITEENABLE2, D3DRS_COLORWRITEENABLE3,
    D3DRS_BLENDFACTOR, D3DRS_SRGBWRITEENABLE, D3DRS_DEPTHBIAS, D3DRS_WRAP8, D3DRS_WRAP9, D3DRS_WRAP10,
    D3DRS_WRAP11, D3DRS_WRAP12, D3DRS_WRAP13, D3DRS_WRAP14, D3DRS_WRAP15, D3DRS_SEPARATEALPHABLENDENABLE,
    D3DRS_SRCBLENDALPHA, D3DRS_DESTBLENDALPHA, D3DRS_BLENDOPALPHA,
};

const D3DSAMPLERSTATETYPE kEverySamplerState[] = {
    D3DSAMP_ADDRESSU,      D3DSAMP_ADDRESSV,      D3DSAMP_ADDRESSW,      D3DSAMP_BORDERCOLOR, D3DSAMP_MAGFILTER,
    D3DSAMP_MINFILTER,     D3DSAMP_MIPFILTER,     D3DSAMP_MIPMAPLODBIAS, D3DSAMP_MAXMIPLEVEL, D3DSAMP_MAXANISOTROPY,
    D3DSAMP_SRGBTEXTURE,   D3DSAMP_ELEMENTINDEX,  D3DSAMP_DMAPOFFSET,
};

const DWORD kVertexSamplers[] = {D3DVERTEXTEXTURESAMPLER0, D3DVERTEXTEXTURESAMPLER1, D3DVERTEXTEXTURESAMPLER2,
                                 D3DVERTEXTEXTURESAMPLER3};

template <size_t Count>
std::vector<WaterFftTile> TileList(const WaterFftTile (&tiles)[Count])
{
    return std::vector<WaterFftTile>(tiles, tiles + Count);
}

uint32_t EveryTile(size_t count)
{
    return (1u << count) - 1u;
}

WaterFftSettings SettingsFor(int resolution, int referenceResolution = kReferenceResolution)
{
    return {resolution, referenceResolution, kWindSpeed, {1.0f, 0.0f}};
}

class Lcg
{
public:
    explicit Lcg(uint32_t seed) : m_state(seed) {}

    double Next()
    {
        m_state = m_state * 1664525u + 1013904223u;
        return static_cast<double>(m_state >> 8) / 8388608.0 - 1.0;
    }

private:
    uint32_t m_state;
};

float HalfToFloat(uint16_t half)
{
    const int exponent = (half >> 10) & 31;
    const int mantissa = half & 1023;
    const float sign = (half & 0x8000) ? -1.0f : 1.0f;
    if (exponent == 31)
        return mantissa ? std::numeric_limits<float>::quiet_NaN() : sign * std::numeric_limits<float>::infinity();
    if (exponent == 0)
        return sign * std::ldexp(static_cast<float>(mantissa), -24);
    return sign * std::ldexp(static_cast<float>(mantissa + 1024), exponent - 25);
}

struct Readback
{
    UINT width = 0;
    UINT height = 0;
    std::vector<unsigned char> raw;
    std::vector<double> values;

    const double* At(UINT x, UINT y) const { return &values[(static_cast<size_t>(y) * width + x) * 4]; }
};

double FoamState(const double* texel, int channel)
{
    return texel[channel] + texel[channel + 2];
}

bool ReadLevel(IDirect3DDevice9* dev, IDirect3DTexture9* texture, UINT level, Readback& out)
{
    out = Readback();
    if (!texture)
        return false;
    IDirect3DSurface9* surface = nullptr;
    IDirect3DSurface9* copy = nullptr;
    D3DSURFACE_DESC desc = {};
    bool read = SUCCEEDED(texture->GetSurfaceLevel(level, &surface)) && SUCCEEDED(surface->GetDesc(&desc)) &&
                (desc.Format == D3DFMT_A16B16G16R16F || desc.Format == D3DFMT_A32B32G32R32F) &&
                SUCCEEDED(dev->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM,
                                                           &copy, nullptr)) &&
                SUCCEEDED(dev->GetRenderTargetData(surface, copy));
    D3DLOCKED_RECT locked = {};
    read = read && SUCCEEDED(copy->LockRect(&locked, nullptr, D3DLOCK_READONLY));
    if (read)
    {
        const bool half = desc.Format == D3DFMT_A16B16G16R16F;
        const size_t channelBytes = half ? sizeof(uint16_t) : sizeof(float);
        const size_t rowBytes = desc.Width * 4 * channelBytes;
        out.width = desc.Width;
        out.height = desc.Height;
        out.raw.resize(rowBytes * desc.Height);
        out.values.resize(static_cast<size_t>(desc.Width) * desc.Height * 4);
        for (UINT y = 0; y < desc.Height; ++y)
        {
            const BYTE* row = static_cast<const BYTE*>(locked.pBits) + static_cast<size_t>(y) * locked.Pitch;
            std::memcpy(&out.raw[y * rowBytes], row, rowBytes);
            for (size_t channel = 0; channel < static_cast<size_t>(desc.Width) * 4; ++channel)
            {
                double value = 0.0;
                if (half)
                {
                    uint16_t bits = 0;
                    std::memcpy(&bits, row + channel * channelBytes, sizeof(bits));
                    value = HalfToFloat(bits);
                }
                else
                {
                    float bits = 0.0f;
                    std::memcpy(&bits, row + channel * channelBytes, sizeof(bits));
                    value = bits;
                }
                out.values[static_cast<size_t>(y) * desc.Width * 4 + channel] = value;
            }
        }
        copy->UnlockRect();
    }
    if (copy)
        copy->Release();
    if (surface)
        surface->Release();
    return read;
}

bool UploadTexels(IDirect3DDevice9* dev, IDirect3DTexture9* target, const std::vector<float>& texels)
{
    D3DSURFACE_DESC desc = {};
    IDirect3DTexture9* staging = nullptr;
    if (!target || FAILED(target->GetLevelDesc(0, &desc)) ||
        texels.size() != static_cast<size_t>(desc.Width) * desc.Height * 4 ||
        FAILED(dev->CreateTexture(desc.Width, desc.Height, 1, 0, desc.Format, D3DPOOL_SYSTEMMEM, &staging, nullptr)))
        return false;
    D3DLOCKED_RECT locked = {};
    bool uploaded = SUCCEEDED(staging->LockRect(0, &locked, nullptr, 0));
    if (uploaded)
    {
        const size_t rowFloats = static_cast<size_t>(desc.Width) * 4;
        for (UINT y = 0; y < desc.Height; ++y)
            std::memcpy(static_cast<BYTE*>(locked.pBits) + static_cast<size_t>(y) * locked.Pitch,
                        &texels[y * rowFloats], rowFloats * sizeof(float));
        uploaded = SUCCEEDED(staging->UnlockRect(0));
    }
    uploaded = uploaded && SUCCEEDED(dev->UpdateTexture(staging, target));
    staging->Release();
    return uploaded;
}

void EnterCallerState(IDirect3DDevice9* dev)
{
    dev->SetDepthStencilSurface(nullptr);
    for (int index = 1; index <= kExtraTargets; ++index)
        dev->SetRenderTarget(index, nullptr);
    for (const RenderStateValue& setting : kCallerRenderStates)
        dev->SetRenderState(setting.state, setting.value);
}

bool SimulateFrame(IDirect3DDevice9* dev, WaterFft& fft, const WaterFftSettings& settings,
                   const std::vector<WaterFftTile>& tiles, uint32_t mask, double seconds, float deltaSeconds)
{
    EnterCallerState(dev);
    if (FAILED(dev->BeginScene()))
        return false;
    const bool simulated = fft.Simulate(dev, settings, tiles, mask, seconds, deltaSeconds);
    dev->EndScene();
    return simulated;
}

bool BeginPasses(IDirect3DDevice9* dev, WaterFftPasses& passes)
{
    EnterCallerState(dev);
    if (FAILED(dev->BeginScene()))
        return false;
    passes.Begin(dev);
    return true;
}

void EndPasses(IDirect3DDevice9* dev, WaterFftPasses& passes)
{
    passes.End();
    dev->EndScene();
}

size_t TexelIndex(int x, int y, int resolution)
{
    return static_cast<size_t>(y) * resolution + x;
}

std::vector<WaterDisplacement> SlotDisplacement(const Readback& atlas, int resolution, int slot)
{
    std::vector<WaterDisplacement> displacement(static_cast<size_t>(resolution) * resolution);
    for (int y = 0; y < resolution; ++y)
        for (int x = 0; x < resolution; ++x)
        {
            const double* texel = atlas.At(static_cast<UINT>(slot * resolution + x), static_cast<UINT>(y));
            displacement[TexelIndex(x, y, resolution)] = {texel[0], texel[1], texel[2], texel[3]};
        }
    return displacement;
}

bool EvolveOnGpu(IDirect3DDevice9* dev, WaterFftPasses& passes, const std::vector<WaterFftTile>& tiles,
                 int resolution, double seconds, std::vector<std::vector<WaterDisplacement>>& out)
{
    const int count = static_cast<int>(tiles.size());
    if (!passes.Prepare(dev, resolution, count) || !BeginPasses(dev, passes))
        return false;
    for (int slot = 0; slot < count; ++slot)
        passes.Evolve(slot, tiles[slot], SettingsFor(resolution), seconds);
    passes.Transform(count, kReferenceResolution);
    EndPasses(dev, passes);
    Readback atlas;
    if (!ReadLevel(dev, passes.Displacement(), 0, atlas))
        return false;
    out.clear();
    for (int slot = 0; slot < count; ++slot)
        out.push_back(SlotDisplacement(atlas, resolution, slot));
    return true;
}

double LargestDisplacement(const std::vector<WaterDisplacement>& displacement)
{
    double largest = 0.0;
    for (const WaterDisplacement& d : displacement)
        largest = std::max({largest, std::fabs(d.x), std::fabs(d.y), std::fabs(d.up)});
    return largest;
}

double DisplacementError(const std::vector<WaterDisplacement>& a, const std::vector<WaterDisplacement>& b)
{
    double worst = 0.0;
    for (size_t i = 0; i < a.size(); ++i)
        worst = std::max({worst, std::fabs(a[i].x - b[i].x), std::fabs(a[i].y - b[i].y),
                          std::fabs(a[i].up - b[i].up)});
    return worst;
}

double LargestResidual(const std::vector<WaterDisplacement>& displacement)
{
    double largest = 0.0;
    for (const WaterDisplacement& d : displacement)
        largest = std::max(largest, std::fabs(d.residual));
    return largest;
}

void DirectInverseDft(std::vector<WaterComplex>& grid, int resolution)
{
    std::vector<WaterComplex> turns(resolution);
    for (int i = 0; i < resolution; ++i)
        turns[i] = std::polar(1.0, kExactTurn * i / resolution);
    std::vector<WaterComplex> line(resolution);
    auto transform = [&](auto index) {
        for (int out = 0; out < resolution; ++out)
        {
            WaterComplex sum = 0.0;
            for (int in = 0; in < resolution; ++in)
                sum += grid[index(in)] * turns[(in * out) % resolution];
            line[out] = sum;
        }
        for (int out = 0; out < resolution; ++out)
            grid[index(out)] = line[out];
    };
    for (int y = 0; y < resolution; ++y)
        transform([&](int x) { return TexelIndex(x, y, resolution); });
    for (int x = 0; x < resolution; ++x)
        transform([&](int y) { return TexelIndex(x, y, resolution); });
}

double LargestMagnitude(const std::vector<WaterComplex>& values)
{
    double largest = 0.0;
    for (const WaterComplex& value : values)
        largest = std::max(largest, std::abs(value));
    return largest;
}

double LargestDifference(const std::vector<WaterComplex>& a, const std::vector<WaterComplex>& b)
{
    double worst = 0.0;
    for (size_t i = 0; i < a.size(); ++i)
        worst = std::max(worst, std::abs(a[i] - b[i]));
    return worst;
}

std::vector<WaterComplex> RandomGrid(Lcg& random, int resolution)
{
    std::vector<WaterComplex> grid(static_cast<size_t>(resolution) * resolution);
    for (WaterComplex& value : grid)
        value = WaterComplex(random.Next(), random.Next());
    return grid;
}

void CheckButterfliesInvertTheDft()
{
    Lcg random(kRandomSeed);
    double worst = 0.0;
    for (int resolution : {8, 16, 128, 256})
    {
        const std::vector<WaterComplex> input = RandomGrid(random, resolution);
        std::vector<WaterComplex> butterflies = input;
        std::vector<WaterComplex> direct = input;
        WaterInverseTransform(butterflies, resolution);
        DirectInverseDft(direct, resolution);
        worst = std::max(worst, LargestDifference(butterflies, direct) / LargestMagnitude(direct));
    }
    std::printf("     butterfly table vs direct inverse DFT (N 8..256, double): relative error %.2e\n", worst);
    Check(worst <= kDoubleTolerance, "water FFT butterfly table computes the unnormalised inverse DFT");
}

void CheckSpectrumMatchesForeverReference()
{
    const WaterFftTile& tile = kLakeTiles[1];
    const float wind[2] = {1.0f, 0.0f};
    const WaterSpectrumShape shape = MakeWaterSpectrumShape(tile, kWindSpeed, wind);
    double worst = 0.0;
    for (const GoldenTexel& golden : kTile24GoldenAt256)
    {
        const double kx = WaterWaveNumber(golden.x, kReferenceResolution, shape.length);
        const double ky = WaterWaveNumber(golden.y, kReferenceResolution, shape.length);
        const WaterGaussianPair noise = WaterNoiseAt(golden.x, golden.y, kReferenceResolution);
        const WaterComplex towards = noise.towards * WaterSpectrumAmplitude(kx, ky, shape);
        const WaterComplex away = noise.away * WaterSpectrumAmplitude(-kx, -ky, shape);
        const double h0[4] = {towards.real(), towards.imag(), away.real(), away.imag()};
        for (int channel = 0; channel < 4; ++channel)
            worst = std::max(worst, std::fabs(h0[channel] - golden.h0[channel]) /
                                        std::max(std::fabs(golden.h0[channel]), 1.0));
    }
    std::printf("     tile 24 h0 at N 256 vs the kit's Python reference: relative error %.2e\n", worst);
    Check(worst <= kGoldenTolerance, "water spectrum h0 reproduces the Forever reference texels of tile 24");

    const std::vector<float> noise = WaterSpectrumNoise(kReferenceResolution);
    const std::vector<float> again = WaterSpectrumNoise(kReferenceResolution);
    double sum[3] = {};
    double squares[3] = {};
    const size_t texels = noise.size() / kWaterFftTexelChannels;
    for (size_t i = 0; i < texels; ++i)
        for (int channel = 0; channel < 3; ++channel)
        {
            const double value = noise[i * kWaterFftTexelChannels + channel];
            sum[channel] += value;
            squares[channel] += value * value;
        }
    bool gaussian = true;
    for (int channel = 0; channel < 3; ++channel)
    {
        const double mean = sum[channel] / texels;
        const double variance = squares[channel] / texels - mean * mean;
        std::printf("     noise channel %d: mean %.4f variance %.4f\n", channel, mean, variance);
        gaussian = gaussian && std::fabs(mean) < kNoiseMeanTolerance &&
                   std::fabs(variance - 1.0) < kNoiseVarianceTolerance;
    }
    Check(noise == again && gaussian,
          "water spectrum noise is deterministic with zero-mean, unit-variance Box-Muller channels");
}

std::vector<WaterDisplacement> SeparateForeverTransforms(const WaterModeField& modes, int resolution,
                                                         int referenceResolution)
{
    std::vector<WaterComplex> height = modes.height;
    std::vector<WaterComplex> alongX = modes.x;
    std::vector<WaterComplex> alongY = modes.y;
    WaterInverseTransform(height, resolution);
    WaterInverseTransform(alongX, resolution);
    WaterInverseTransform(alongY, resolution);
    const double area = static_cast<double>(referenceResolution) * referenceResolution;
    std::vector<WaterDisplacement> displacement(height.size());
    for (int y = 0; y < resolution; ++y)
        for (int x = 0; x < resolution; ++x)
        {
            const size_t i = TexelIndex(x, y, resolution);
            const double scale = ((x + y) & 1 ? -1.0 : 1.0) / area;
            displacement[i] = {scale * alongX[i].real(), scale * alongY[i].real(), -scale * height[i].real(), 0.0};
        }
    return displacement;
}

void CheckFoldMatchesSeparateTransforms()
{
    const int resolution = 64;
    const float wind[2] = {1.0f, 0.0f};
    double worst = 0.0;
    double residual = 0.0;
    for (const WaterFftTile& tile : kLakeTiles)
    {
        const WaterSpectrumShape shape = MakeWaterSpectrumShape(tile, kWindSpeed, wind);
        const std::vector<WaterForeverAmplitude> h0 =
            WaterReferenceH0(WaterSpectrumNoise(resolution), resolution, shape);
        const WaterModeField modes =
            WaterReferenceModes(h0, resolution, shape.length, MakeWaterLoopClock(kSimulatedSeconds).fraction);
        const std::vector<WaterDisplacement> separate =
            SeparateForeverTransforms(modes, resolution, kReferenceResolution);
        WaterPackedSpectrum packed = WaterFoldModes(modes, resolution);
        WaterInverseTransform(packed.heightAndX, resolution);
        WaterInverseTransform(packed.y, resolution);
        const std::vector<WaterDisplacement> folded =
            WaterAssembleDisplacement(packed, resolution, kReferenceResolution);
        const double scale = LargestDisplacement(separate);
        worst = std::max(worst, DisplacementError(folded, separate) / scale);
        residual = std::max(residual, LargestResidual(folded) / scale);
    }
    std::printf("     Hermitian fold vs three Forever transforms (lake tiles, N 64): relative error %.2e, "
                "residual %.2e\n",
                worst, residual);
    Check(worst <= kDoubleTolerance && residual <= kDoubleTolerance,
          "water FFT packs height and both displacements into one transform without changing Forever's result");
}

struct SingleModeExpectation
{
    double up;
    double x;
    double slope;
    double jacobian;
};

SingleModeExpectation ExpectedSingleMode(int texel)
{
    const double waveNumber = kExactTurn * kSingleModeCycles / kSingleModeLength;
    const double spacing = static_cast<double>(kSingleModeLength) / kSingleModeResolution;
    const double phase = kExactTurn * kSingleModeCycles * texel / kSingleModeResolution;
    const double differenceScale = std::sin(waveNumber * spacing) / spacing;
    return {-kSingleModeAmplitude * std::cos(phase), kSingleModeAmplitude * std::sin(phase),
            kSingleModeAmplitude * std::sin(phase) * differenceScale,
            1.0 + kSingleModeAmplitude * std::cos(phase) * differenceScale};
}

int SingleModeCrestTexel()
{
    int crest = 0;
    for (int x = 1; x < kSingleModeResolution; ++x)
        if (ExpectedSingleMode(x).up > ExpectedSingleMode(crest).up)
            crest = x;
    return crest;
}

double SingleModeSpectrumValue()
{
    return kSingleModeAmplitude * kSingleModeResolution * kSingleModeResolution;
}

void CheckReferenceSingleMode()
{
    const int n = kSingleModeResolution;
    WaterPackedSpectrum packed = {std::vector<WaterComplex>(static_cast<size_t>(n) * n),
                                  std::vector<WaterComplex>(static_cast<size_t>(n) * n)};
    packed.heightAndX[TexelIndex(n / 2 + kSingleModeCycles, n / 2, n)] = SingleModeSpectrumValue();
    WaterInverseTransform(packed.heightAndX, n);
    WaterInverseTransform(packed.y, n);
    const std::vector<WaterDisplacement> displacement = WaterAssembleDisplacement(packed, n, n);
    const std::vector<WaterSurfaceTexel> surface = WaterReferenceSurface(displacement, n, kSingleModeLength);
    double worst = 0.0;
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
        {
            const SingleModeExpectation expected = ExpectedSingleMode(x);
            const WaterDisplacement& d = displacement[TexelIndex(x, y, n)];
            const WaterSurfaceTexel& s = surface[TexelIndex(x, y, n)];
            worst = std::max({worst, std::fabs(d.up - expected.up), std::fabs(d.x - expected.x), std::fabs(d.y),
                              std::fabs(s.slope[0] - expected.slope), std::fabs(s.jacobian - expected.jacobian)});
        }
    const int crest = SingleModeCrestTexel();
    const double crestJacobian = surface[TexelIndex(crest, 0, n)].jacobian;
    const double troughJacobian = surface[TexelIndex(0, 0, n)].jacobian;
    std::printf("     reference single mode: error %.2e, crest texel %d J %.4f, trough J %.4f\n", worst, crest,
                crestJacobian, troughJacobian);
    Check(worst <= kDoubleTolerance && crestJacobian < 1.0 && troughJacobian > 1.0,
          "water reference single mode gives Up = -(A/Nref^2) cos kx, Dx = +(A/Nref^2) sin kx and crest compression");
}

void CheckFoamOdeReachesEquilibrium()
{
    const float* swamp = kStillSwampTile.foam;
    double foam = 0.0;
    for (int frame = 0; frame < kEquilibriumFrames; ++frame)
        foam = WaterFoamStep(foam, 1.0, swamp, kLongestFoamStepSeconds);
    const double swampEquilibrium = static_cast<double>(swamp[1]) * swamp[0] / (swamp[1] * swamp[0] + swamp[2]);
    const float lake[3] = {-0.16f, 12.0f, 3.0f};
    const double jacobian = 0.5;
    const double injection = lake[0] + 1.0 - jacobian;
    const double previous = 0.3;
    const double euler = previous + kFrameSeconds * (lake[1] * injection * (1.0 - previous) - lake[2] * previous);
    const double step = WaterFoamStep(previous, jacobian, lake, kFrameSeconds);
    double compressed = 0.0;
    for (int frame = 0; frame < kEquilibriumFrames; ++frame)
        compressed = WaterFoamStep(compressed, jacobian, lake, kLongestFoamStepSeconds);
    const double compressedEquilibrium = lake[1] * injection / (lake[1] * injection + lake[2]);
    std::printf("     foam ODE: swamp %.6f (expected %.6f), J 0.5 lake %.6f (expected %.6f)\n", foam,
                swampEquilibrium, compressed, compressedEquilibrium);
    Check(std::fabs(foam - swampEquilibrium) < kEquilibriumTolerance &&
              std::fabs(swampEquilibrium - kSwampFoamEquilibrium) < kEquilibriumTolerance &&
              std::fabs(compressed - compressedEquilibrium) < kEquilibriumTolerance &&
              std::fabs(step - euler) < kDoubleTolerance,
          "water foam update is the explicit Euler step and settles at rate*injection/(rate*injection+decay)");
}

void CheckGpuButterfliesInvertTheDft(IDirect3DDevice9* dev)
{
    struct Layout
    {
        int resolution;
        int slots;
    };
    Lcg random(kRandomSeed + 1);
    bool ran = true;
    double worstRatio = 0.0;
    for (const Layout layout : {Layout{kWaterFftLowResolution, 3}, Layout{kWaterFftHighResolution, 1}})
    {
        const int n = layout.resolution;
        WaterFftPasses passes;
        if (!passes.Prepare(dev, n, layout.slots))
        {
            ran = false;
            break;
        }
        const int atlasSlots = passes.Slots();
        std::vector<float> texels(static_cast<size_t>(atlasSlots) * n * n * 4);
        for (float& texel : texels)
            texel = static_cast<float>(random.Next());
        Readback atlas;
        ran = ran && UploadTexels(dev, passes.Spectrum(), texels) && BeginPasses(dev, passes);
        if (!ran)
            break;
        passes.Transform(layout.slots, n);
        EndPasses(dev, passes);
        ran = ReadLevel(dev, passes.Displacement(), 0, atlas);
        if (!ran)
            break;
        const double area = static_cast<double>(n) * n;
        for (int slot = 0; slot < layout.slots; ++slot)
        {
            std::vector<WaterComplex> first(static_cast<size_t>(n) * n);
            std::vector<WaterComplex> second(first.size());
            for (int y = 0; y < n; ++y)
                for (int x = 0; x < n; ++x)
                {
                    const float* texel = &texels[(static_cast<size_t>(y) * atlasSlots * n + slot * n + x) * 4];
                    first[TexelIndex(x, y, n)] = WaterComplex(texel[0], texel[1]);
                    second[TexelIndex(x, y, n)] = WaterComplex(texel[2], texel[3]);
                }
            DirectInverseDft(first, n);
            DirectInverseDft(second, n);
            double worst = 0.0;
            for (int y = 0; y < n; ++y)
                for (int x = 0; x < n; ++x)
                {
                    const double* texel = atlas.At(static_cast<UINT>(slot * n + x), static_cast<UINT>(y));
                    const double unscale = ((x + y) & 1 ? -1.0 : 1.0) * area;
                    const WaterComplex gpuFirst(-texel[2] * unscale, texel[0] * unscale);
                    const WaterComplex gpuSecond(texel[1] * unscale, texel[3] * unscale);
                    worst = std::max({worst, std::abs(gpuFirst - first[TexelIndex(x, y, n)]),
                                      std::abs(gpuSecond - second[TexelIndex(x, y, n)])});
                }
            const double relative = worst / std::max(LargestMagnitude(first), LargestMagnitude(second));
            const double allowed = kFloatTolerancePerStage * WaterFftStages(n);
            worstRatio = std::max(worstRatio, relative / allowed);
            std::printf("     GPU butterflies N %d slot %d of %d: relative error %.2e (allowed %.1e)\n", n, slot,
                        layout.slots, relative, allowed);
        }
    }
    Check(ran && worstRatio <= 1.0,
          "water GPU butterfly passes compute the inverse DFT of every atlas slot (N 128 x3 slots, N 256)");
}

void CheckGpuSingleMode(IDirect3DDevice9* dev)
{
    const int n = kSingleModeResolution;
    WaterFftPasses passes;
    IDirect3DTexture9* surface = nullptr;
    IDirect3DTexture9* foam[2] = {};
    bool ran = passes.Prepare(dev, n, 1) && passes.CreateSurfaceMap(&surface) && passes.CreateFoamMap(&foam[0]) &&
               passes.CreateFoamMap(&foam[1]);
    std::vector<float> texels(static_cast<size_t>(passes.Slots()) * n * n * 4);
    texels[(static_cast<size_t>(n / 2) * passes.Slots() * n + n / 2 + kSingleModeCycles) * 4] =
        static_cast<float>(SingleModeSpectrumValue());
    ran = ran && UploadTexels(dev, passes.Spectrum(), texels) && BeginPasses(dev, passes);
    if (ran)
    {
        passes.Transform(1, n);
        passes.ClearFoam(foam[0]);
        passes.UpdateMaps(0, kCrestFoamTile, kCrestFoamStepSeconds, surface, foam[0], foam[1]);
        EndPasses(dev, passes);
    }
    Readback displacement;
    Readback moments;
    Readback foamState;
    ran = ran && ReadLevel(dev, passes.Displacement(), 0, displacement) && ReadLevel(dev, surface, 0, moments) &&
          ReadLevel(dev, foam[1], 0, foamState);
    double displacementError = 0.0;
    double slopeError = 0.0;
    double foamError = 0.0;
    for (int y = 0; ran && y < n; ++y)
        for (int x = 0; x < n; ++x)
        {
            const SingleModeExpectation expected = ExpectedSingleMode(x);
            const double* d = displacement.At(static_cast<UINT>(x), static_cast<UINT>(y));
            const double* m = moments.At(static_cast<UINT>(x), static_cast<UINT>(y));
            const double* f = foamState.At(static_cast<UINT>(x), static_cast<UINT>(y));
            displacementError = std::max({displacementError, std::fabs(d[0] - expected.x), std::fabs(d[1]),
                                          std::fabs(d[2] - expected.up)});
            slopeError = std::max({slopeError, std::fabs(m[0] - expected.slope), std::fabs(m[1])});
            const double expectedFoam =
                kCrestFoamStepSeconds * std::max(kCrestFoamTile.foam[0] + 1.0 - expected.jacobian, 0.0);
            foamError = std::max(foamError, std::fabs(FoamState(f, 0) - expectedFoam));
        }
    const int crest = SingleModeCrestTexel();
    const double crestFoam = ran ? FoamState(foamState.At(static_cast<UINT>(crest), 0), 0) : 0.0;
    const double troughFoam = ran ? FoamState(foamState.At(0, 0), 0) : 1.0;
    std::printf("     GPU single mode: displacement error %.2e, slope error %.2e, foam error %.2e, crest foam %.4f, "
                "trough foam %.4f\n",
                displacementError, slopeError, foamError, crestFoam, troughFoam);
    Check(ran && displacementError <= kFloatTolerance * kSingleModeAmplitude,
          "water GPU single mode gives Up = -(A/Nref^2) cos kx and Dx = +(A/Nref^2) sin kx");
    Check(ran && slopeError <= kHalfRelativeTolerance && foamError <= kFoamStateTolerance && crestFoam > 0.0 &&
              troughFoam == 0.0,
          "water GPU slope matches the discrete derivative and foam injects only where crests compress (J < 1)");
    for (IUnknown* resource : std::initializer_list<IUnknown*>{surface, foam[0], foam[1]})
        if (resource)
            resource->Release();
}

std::vector<WaterComplex> FoldedHeights(const WaterFftTile& tile, int resolution, double seconds)
{
    const float wind[2] = {1.0f, 0.0f};
    const WaterSpectrumShape shape = MakeWaterSpectrumShape(tile, kWindSpeed, wind);
    WaterModeField modes =
        WaterReferenceModes(WaterReferenceH0(WaterSpectrumNoise(resolution), resolution, shape), resolution,
                            shape.length, MakeWaterLoopClock(seconds).fraction);
    std::fill(modes.x.begin(), modes.x.end(), WaterComplex());
    std::fill(modes.y.begin(), modes.y.end(), WaterComplex());
    return WaterFoldModes(modes, resolution).heightAndX;
}

void CheckGpuMatchesReference(IDirect3DDevice9* dev)
{
    const float wind[2] = {1.0f, 0.0f};
    bool ran = true;
    double worst = 0.0;
    double residual = 0.0;
    double parseval = 0.0;
    double mean = 0.0;
    for (int resolution : {kWaterFftLowResolution, kWaterFftHighResolution})
        for (const std::vector<WaterFftTile>& tiles : {TileList(kLakeTiles), TileList(kOceanTiles)})
        {
            WaterFftPasses passes;
            std::vector<std::vector<WaterDisplacement>> gpu;
            if (!EvolveOnGpu(dev, passes, tiles, resolution, kSimulatedSeconds, gpu))
            {
                ran = false;
                continue;
            }
            for (size_t slot = 0; slot < tiles.size(); ++slot)
            {
                const std::vector<WaterDisplacement> reference = WaterReferenceDisplacement(
                    tiles[slot], resolution, kReferenceResolution, kWindSpeed, wind, kSimulatedSeconds);
                const double scale = LargestDisplacement(reference);
                const double error = DisplacementError(gpu[slot], reference) / scale;
                worst = std::max(worst, error);
                residual = std::max(residual, LargestResidual(gpu[slot]) / scale);
                double sumSquares = 0.0;
                double sum = 0.0;
                for (const WaterDisplacement& d : gpu[slot])
                {
                    sumSquares += d.up * d.up;
                    sum += d.up;
                }
                double spectral = 0.0;
                for (const WaterComplex& height : FoldedHeights(tiles[slot], resolution, kSimulatedSeconds))
                    spectral += std::norm(height);
                const double area = static_cast<double>(resolution) * resolution;
                const double referenceArea = static_cast<double>(kReferenceResolution) * kReferenceResolution;
                const double expected = area * spectral / (referenceArea * referenceArea);
                parseval = std::max(parseval, std::fabs(sumSquares - expected) / expected);
                mean = std::max(mean, std::fabs(sum / area) / scale);
                std::printf("     GPU tile %3u N %d: error %.2e of %.3f yd, h rms %.4f yd\n", tiles[slot].foreverTileId,
                            resolution, error, scale, std::sqrt(sumSquares / area));
            }
        }
    std::printf("     GPU vs double reference: worst %.2e, residual %.2e, Parseval %.2e, mean %.2e\n", worst,
                residual, parseval, mean);
    Check(ran && worst <= kFloatTolerance && residual <= kFloatTolerance,
          "water GPU evolution, fold and transforms match the double reference (lake and ocean tiles, N 128 and 256)");
    Check(ran && parseval <= kParsevalTolerance && mean <= kFloatTolerance,
          "water GPU heights keep Parseval's energy (N^2/Nref^4 sum |Yh|^2) and a zero mean");
}

void CheckKitStatistics(IDirect3DDevice9* dev)
{
    std::vector<WaterFftTile> tiles = TileList(kLakeTiles);
    tiles.insert(tiles.end(), std::begin(kOceanTiles), std::end(kOceanTiles));
    WaterFftPasses passes;
    std::vector<std::vector<WaterDisplacement>> gpu;
    const bool ran = EvolveOnGpu(dev, passes, tiles, kWaterFftHighResolution, kStatisticsSeconds, gpu);
    bool matches = ran;
    std::printf("     tile |  h rms  | kit h rms |  h max  | slope rms | J min (N 256, wind 2, t 10 s)\n");
    for (size_t slot = 0; ran && slot < tiles.size(); ++slot)
    {
        const std::vector<WaterSurfaceTexel> surface =
            WaterReferenceSurface(gpu[slot], kWaterFftHighResolution, WaterTileLength(tiles[slot]));
        double squares = 0.0;
        double largest = 0.0;
        double slopes = 0.0;
        double jacobian = std::numeric_limits<double>::max();
        for (size_t i = 0; i < surface.size(); ++i)
        {
            squares += gpu[slot][i].up * gpu[slot][i].up;
            largest = std::max(largest, std::fabs(gpu[slot][i].up));
            slopes += surface[i].slopeSquared;
            jacobian = std::min(jacobian, surface[i].jacobian);
        }
        const double rms = std::sqrt(squares / surface.size());
        double kit = 0.0;
        for (const KitHeightRms& row : kKitHeightRmsAt256)
            if (row.tile == tiles[slot].foreverTileId)
                kit = row.heightRms;
        matches = matches && std::fabs(rms - kit) <= kKitStatisticsTolerance * kit;
        std::printf("     %4u | %.4f  |   %.3f   | %.4f  |  %.4f   | %.3f\n", tiles[slot].foreverTileId, rms, kit,
                    largest, std::sqrt(slopes / surface.size()), jacobian);
    }
    Check(matches, "water GPU wave heights agree with the kit's per-tile statistics (within 15%)");
}

struct TileReadback
{
    Readback surface;
    Readback foam;
};

bool ReadTile(IDirect3DDevice9* dev, const WaterFft& fft, int tile, TileReadback& out)
{
    return ReadLevel(dev, fft.Surface(tile), 0, out.surface) && ReadLevel(dev, fft.Foam(tile), 0, out.foam);
}

double HalfError(double actual, double expected, double scale)
{
    return std::fabs(actual - expected) / (kHalfRelativeTolerance * std::fabs(expected) +
                                           kHalfAbsoluteTolerance * std::max(scale, 1.0e-6));
}

double HalfStorageError(double actual, double expected)
{
    return std::fabs(actual - expected) / (kHalfRelativeTolerance * std::fabs(expected) + kSmallestHalfStep);
}

void CheckSimulatedMapsMatchReference(IDirect3DDevice9* dev)
{
    const int n = kWaterFftLowResolution;
    const std::vector<WaterFftTile> tiles = TileList(kLakeTiles);
    WaterFft fft;
    WaterFftPasses passes;
    std::vector<std::vector<WaterDisplacement>> gpu;
    bool ran = SimulateFrame(dev, fft, SettingsFor(n), tiles, EveryTile(tiles.size()), kSimulatedSeconds,
                             kFrameSeconds) &&
               EvolveOnGpu(dev, passes, tiles, n, kSimulatedSeconds, gpu);
    double interior = 0.0;
    double boundary = 0.0;
    double foamError = 0.0;
    for (size_t tile = 0; ran && tile < tiles.size(); ++tile)
    {
        TileReadback maps;
        if (!ReadTile(dev, fft, static_cast<int>(tile), maps))
        {
            ran = false;
            break;
        }
        const std::vector<WaterSurfaceTexel> reference =
            WaterReferenceSurface(gpu[tile], n, WaterTileLength(tiles[tile]));
        double scale[4] = {};
        for (const WaterSurfaceTexel& texel : reference)
        {
            scale[0] = std::max(scale[0], std::fabs(texel.slope[0]));
            scale[1] = std::max(scale[1], std::fabs(texel.slope[1]));
            scale[2] = std::max(scale[2], texel.slopeSquared);
            scale[3] = std::max(scale[3], texel.wideSlopeSquared);
        }
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x)
            {
                const WaterSurfaceTexel& expected = reference[TexelIndex(x, y, n)];
                const double* actual = maps.surface.At(static_cast<UINT>(x), static_cast<UINT>(y));
                const double wanted[4] = {expected.slope[0], expected.slope[1], expected.slopeSquared,
                                          expected.wideSlopeSquared};
                double error = 0.0;
                for (int channel = 0; channel < 4; ++channel)
                    error = std::max(error, HalfError(actual[channel], wanted[channel], scale[channel]));
                const bool onEdge = x == 0 || y == 0 || x == n - 1 || y == n - 1;
                double& region = onEdge ? boundary : interior;
                region = std::max(region, error);
                const double* foam = maps.foam.At(static_cast<UINT>(x), static_cast<UINT>(y));
                const double foamExpected =
                    WaterFoamStep(0.0, expected.jacobian, tiles[tile].foam, kFrameSeconds);
                const double oxygenExpected =
                    WaterFoamStep(0.0, expected.jacobian, tiles[tile].oxygen, kFrameSeconds);
                foamError = std::max({foamError, std::fabs(FoamState(foam, 0) - foamExpected),
                                      std::fabs(FoamState(foam, 1) - oxygenExpected)});
            }
    }
    std::printf("     simulated moments vs reference (fraction of fp16 tolerance): interior %.3f, periodic edges %.3f; "
                "foam error %.2e\n",
                interior, boundary, foamError);
    Check(ran && interior <= 1.0 && boundary <= 1.0,
          "water surface map holds Forever's gradient moments, wrapping periodically at texels 0 and N-1");
    Check(ran && foamError <= kFoamStateTolerance,
          "water foam and oxygen maps take one explicit Euler step of the Jacobian-driven ODE");

    double levelError = 0.0;
    double chainEnd = 0.0;
    bool mipsRead = ran;
    for (size_t tile = 0; mipsRead && tile < tiles.size(); ++tile)
    {
        IDirect3DTexture9* surface = fft.Surface(static_cast<int>(tile));
        const UINT levels = surface ? surface->GetLevelCount() : 0;
        mipsRead = levels == static_cast<UINT>(WaterFftStages(n) + 1);
        Readback finer;
        mipsRead = mipsRead && ReadLevel(dev, surface, 0, finer);
        double channelScale[4] = {};
        double mean[4] = {};
        for (UINT y = 0; mipsRead && y < finer.height; ++y)
            for (UINT x = 0; x < finer.width; ++x)
                for (int channel = 0; channel < 4; ++channel)
                {
                    channelScale[channel] = std::max(channelScale[channel], std::fabs(finer.At(x, y)[channel]));
                    mean[channel] += finer.At(x, y)[channel] / (static_cast<double>(finer.width) * finer.height);
                }
        for (UINT level = 1; mipsRead && level < levels; ++level)
        {
            Readback coarser;
            mipsRead = ReadLevel(dev, surface, level, coarser) && coarser.width * 2 == finer.width;
            for (UINT y = 0; mipsRead && y < coarser.height; ++y)
                for (UINT x = 0; x < coarser.width; ++x)
                    for (int channel = 0; channel < 4; ++channel)
                    {
                        const double box = 0.25 * (finer.At(2 * x, 2 * y)[channel] +
                                                   finer.At(2 * x + 1, 2 * y)[channel] +
                                                   finer.At(2 * x, 2 * y + 1)[channel] +
                                                   finer.At(2 * x + 1, 2 * y + 1)[channel]);
                        levelError = std::max(levelError, HalfStorageError(coarser.At(x, y)[channel], box));
                    }
            if (mipsRead && coarser.width == 1)
                for (int channel = 0; channel < 4; ++channel)
                    chainEnd = std::max(chainEnd, std::fabs(coarser.At(0, 0)[channel] - mean[channel]) /
                                                      std::max(channelScale[channel], 1.0e-6));
            finer = coarser;
        }
    }
    std::printf("     surface mips: box-average error %.3f of fp16 tolerance, 1x1 level vs level-0 mean %.2e\n",
                levelError, chainEnd);
    Check(mipsRead && levelError <= 1.0 && chainEnd <= kChainEndTolerance,
          "water surface map carries a full mip chain where each level is the 2x2 box average of the one above");
}

bool SameBits(const Readback& a, const Readback& b)
{
    return a.width == b.width && a.height == b.height && a.raw == b.raw;
}

void CheckStepZeroAndRepeatability(IDirect3DDevice9* dev)
{
    const std::vector<WaterFftTile> tiles = TileList(kLakeTiles);
    const WaterFftSettings settings = SettingsFor(kWaterFftLowResolution);
    WaterFft fft;
    double seconds = kSimulatedSeconds;
    bool ran = true;
    for (int frame = 0; ran && frame < 3; ++frame, seconds += kFrameSeconds)
        ran = SimulateFrame(dev, fft, settings, tiles, EveryTile(tiles.size()), seconds, kFrameSeconds);
    seconds -= kFrameSeconds;
    TileReadback before;
    TileReadback after;
    ran = ran && ReadTile(dev, fft, 1, before) &&
          SimulateFrame(dev, fft, settings, tiles, EveryTile(tiles.size()), seconds, 0.0f) &&
          ReadTile(dev, fft, 1, after);
    double foam = 0.0;
    for (size_t i = 0; ran && i < before.foam.values.size(); i += 4)
        foam = std::max(foam, before.foam.values[i]);
    Check(ran && foam > 0.0 && SameBits(before.foam, after.foam),
          "water foam state is bitwise unchanged by a zero time step");
    Check(ran && SameBits(before.surface, after.surface),
          "water surface map is bitwise reproducible for the same simulation time");
}

void CheckStillWater(IDirect3DDevice9* dev)
{
    const std::vector<WaterFftTile> tiles = {kStillSwampTile, kStillLakeTile};
    WaterFft fft;
    bool ran = true;
    double seconds = kSimulatedSeconds;
    for (int frame = 0; ran && frame < kEquilibriumFrames; ++frame, seconds += kLongestFoamStepSeconds)
        ran = SimulateFrame(dev, fft, SettingsFor(kWaterFftLowResolution), tiles, EveryTile(tiles.size()), seconds,
                            1.0f);
    TileReadback swamp;
    TileReadback lake;
    ran = ran && ReadTile(dev, fft, 0, swamp) && ReadTile(dev, fft, 1, lake);
    bool flat = ran;
    double swampFoam[2] = {1.0, 0.0};
    double lakeFoam = 0.0;
    for (size_t i = 0; ran && i < swamp.surface.values.size(); i += 4)
    {
        for (int channel = 0; channel < 4; ++channel)
            flat = flat && swamp.surface.values[i + channel] == 0.0 && lake.surface.values[i + channel] == 0.0;
        swampFoam[0] = std::min(swampFoam[0], FoamState(&swamp.foam.values[i], 0));
        swampFoam[1] = std::max(swampFoam[1], FoamState(&swamp.foam.values[i], 0));
        lakeFoam = std::max({lakeFoam, FoamState(&lake.foam.values[i], 0), FoamState(&lake.foam.values[i], 1),
                             FoamState(&swamp.foam.values[i], 1)});
    }
    std::printf("     still water: swamp foam %.6f..%.6f (expected %.6f), other foam and oxygen up to %.6f\n",
                swampFoam[0], swampFoam[1], kSwampFoamEquilibrium, lakeFoam);
    Check(flat, "water with zero amplitude has an exactly flat surface map (J = 1 everywhere)");
    Check(ran && std::fabs(swampFoam[0] - kSwampFoamEquilibrium) <= kFoamStateTolerance &&
              std::fabs(swampFoam[1] - kSwampFoamEquilibrium) <= kFoamStateTolerance && lakeFoam == 0.0,
          "water foam on still water settles at the positive-bias equilibrium (tile 139: 0.5247) and stays 0 "
          "otherwise");
}

void CheckTileMaskKeepsOtherTiles(IDirect3DDevice9* dev)
{
    const std::vector<WaterFftTile> tiles = TileList(kLakeTiles);
    const WaterFftSettings settings = SettingsFor(kWaterFftLowResolution);
    WaterFft fft;
    TileReadback first[2];
    TileReadback later[2];
    const bool simulated = SimulateFrame(dev, fft, settings, tiles, 0x5u, kSimulatedSeconds, kFrameSeconds);
    const bool skippedIsEmpty = !fft.Surface(1) && !fft.Foam(1) && !fft.Surface(3) && fft.Surface(0) && fft.Foam(2);
    const bool ran = simulated && ReadTile(dev, fft, 0, first[0]) && ReadTile(dev, fft, 2, first[1]) &&
                     SimulateFrame(dev, fft, settings, tiles, 0x2u, kSimulatedSeconds + 1.0, kFrameSeconds) &&
                     ReadTile(dev, fft, 0, later[0]) && ReadTile(dev, fft, 2, later[1]);
    Check(simulated && skippedIsEmpty, "water maps of tiles never simulated are null");
    Check(ran && fft.Surface(1) && fft.Foam(1) && SameBits(first[0].surface, later[0].surface) &&
              SameBits(first[0].foam, later[0].foam) && SameBits(first[1].surface, later[1].surface) &&
              SameBits(first[1].foam, later[1].foam),
          "water tiles outside the tile mask keep their previous surface and foam maps");
}

size_t CountLines(const std::string& text, const char* fragment)
{
    size_t count = 0;
    for (size_t at = text.find(fragment); at != std::string::npos; at = text.find(fragment, at + 1))
        ++count;
    return count;
}

void CheckTileSetChangesLogQuietly(IDirect3DDevice9* dev)
{
    const std::vector<WaterFftTile> tiles = TileList(kLakeTiles);
    WaterFft fft;
    const size_t start = ReadText(g_harnessLog).size();
    auto logged = [start]() {
        const std::string text = ReadText(g_harnessLog);
        return text.size() > start ? text.substr(start) : std::string();
    };
    LogSetLevel(static_cast<int>(LogLevel::Info));
    bool simulated = true;
    for (uint32_t mask : {0x1u, 0x3u, 0x1u, 0x3u, 0x1u})
        simulated = SimulateFrame(dev, fft, SettingsFor(kWaterFftLowResolution), tiles, mask, kSimulatedSeconds,
                                  kFrameSeconds) &&
                    simulated;
    const size_t atInfo = CountLines(logged(), "water waves: ");
    LogSetLevel(static_cast<int>(LogLevel::Debug));
    for (uint32_t mask : {0x3u, 0x1u})
        simulated = SimulateFrame(dev, fft, SettingsFor(kWaterFftLowResolution), tiles, mask, kSimulatedSeconds,
                                  kFrameSeconds) &&
                    simulated;
    const size_t atDebug = CountLines(logged(), "water waves: ");
    LogSetLevel(static_cast<int>(LogLevel::Info));
    simulated = SimulateFrame(dev, fft, SettingsFor(kWaterFftHighResolution), tiles, 0x1u, kSimulatedSeconds,
                              kFrameSeconds) &&
                simulated;
    const size_t afterResolution = CountLines(logged(), "water waves: ");
    std::printf("     wave plan lines: %zu at LogLevel 1 over five tile sets, %zu after two more at LogLevel 2, %zu "
                "after a resolution change\n",
                atInfo, atDebug, afterResolution);
    Check(simulated && atInfo == 1 && atDebug == 3 && afterResolution == 4,
          "wave tile-set changes are logged at LogLevel 2; LogLevel 1 logs the first plan and new resolutions");
}

constexpr double kSmallestComparedMoment = 1e-3;

void CheckAmplitudeScaleScalesTheWaves(IDirect3DDevice9* dev)
{
    const std::vector<WaterFftTile> tiles = TileList(kLakeTiles);
    const WaterFftSettings full = SettingsFor(kWaterFftLowResolution);
    WaterFftSettings lower = full;
    lower.amplitudeScale = 0.25f;
    WaterFft fullWaves;
    WaterFft halfWaves;
    TileReadback a;
    TileReadback b;
    const bool ran = SimulateFrame(dev, fullWaves, full, tiles, 0x1u, kSimulatedSeconds, kFrameSeconds) &&
                     SimulateFrame(dev, halfWaves, lower, tiles, 0x1u, kSimulatedSeconds, kFrameSeconds) &&
                     ReadTile(dev, fullWaves, 0, a) && ReadTile(dev, halfWaves, 0, b);
    double worstSlope = 0.0;
    double worstMoment = 0.0;
    double largestSlope = 0.0;
    const std::vector<double>& fullMoments = a.surface.values;
    const std::vector<double>& lowerMoments = b.surface.values;
    for (size_t i = 0; ran && i + 3 < fullMoments.size() && i + 3 < lowerMoments.size(); i += 4)
    {
        for (size_t c = 0; c < 2; ++c)
        {
            worstSlope = std::max(worstSlope, std::fabs(lowerMoments[i + c] - 0.5 * fullMoments[i + c]));
            largestSlope = std::max(largestSlope, std::fabs(fullMoments[i + c]));
        }
        for (size_t c = 2; c < 4; ++c)
        {
            const double expected = 0.25 * fullMoments[i + c];
            worstMoment = std::max(worstMoment, std::fabs(lowerMoments[i + c] - expected) /
                                                    std::max(kSmallestComparedMoment, std::fabs(fullMoments[i + c])));
        }
    }
    std::printf("     amplitude scale 0.25: worst slope error %.2e (largest slope %.3f), worst moment error %.3f\n",
                worstSlope, largestSlope, worstMoment);
    Check(ran && largestSlope > 0.0 && worstSlope <= 0.01 * largestSlope && worstMoment <= 0.02,
          "the amplitude scale (WaterWaves squared) halves the simulated slopes and quarters their squares at 0.25");
}

void CheckChangedTilesRestart(IDirect3DDevice9* dev)
{
    const std::vector<WaterFftTile> tiles = TileList(kLakeTiles);
    const uint32_t every = EveryTile(tiles.size());
    const WaterFftSettings settings = SettingsFor(kWaterFftLowResolution);
    WaterFft fft;
    bool ran = true;
    double seconds = kSimulatedSeconds;
    for (int frame = 0; ran && frame < 3; ++frame, seconds += kFrameSeconds)
        ran = SimulateFrame(dev, fft, settings, tiles, every, seconds, kFrameSeconds);
    std::vector<WaterFftTile> changed = tiles;
    changed[1].amplitude *= 0.5f;
    ran = ran && SimulateFrame(dev, fft, settings, changed, 0x1u, seconds, kFrameSeconds);
    const bool forgotten = ran && !fft.Surface(1) && !fft.Foam(1) && fft.Surface(0) && fft.Surface(2);
    WaterFft fresh;
    TileReadback restarted;
    TileReadback firstStep;
    ran = ran && SimulateFrame(dev, fft, settings, changed, every, seconds, kFrameSeconds) &&
          SimulateFrame(dev, fresh, settings, changed, every, seconds, kFrameSeconds) &&
          ReadTile(dev, fft, 1, restarted) && ReadTile(dev, fresh, 1, firstStep);
    Check(forgotten && ran && SameBits(restarted.surface, firstStep.surface) &&
              SameBits(restarted.foam, firstStep.foam),
          "water tiles whose parameters change drop their maps and restart their foam from zero");

    WaterFft unitWind;
    WaterFft longWind;
    WaterFftSettings scaled = settings;
    scaled.windDirection[0] = 3.0f;
    TileReadback unit;
    TileReadback longer;
    const bool normalised = SimulateFrame(dev, unitWind, settings, tiles, every, seconds, kFrameSeconds) &&
                            SimulateFrame(dev, longWind, scaled, tiles, every, seconds, kFrameSeconds) &&
                            ReadTile(dev, unitWind, 1, unit) && ReadTile(dev, longWind, 1, longer) &&
                            SameBits(unit.surface, longer.surface);
    Check(normalised, "water wind direction is normalised before it shapes the spectrum");
}

UINT LevelWidth(IDirect3DTexture9* texture)
{
    D3DSURFACE_DESC desc = {};
    return texture && SUCCEEDED(texture->GetLevelDesc(0, &desc)) ? desc.Width : 0;
}

void CheckResolutionsAndRecreation(IDirect3DDevice9* dev)
{
    const std::vector<WaterFftTile> tiles = TileList(kLakeTiles);
    const uint32_t every = EveryTile(tiles.size());
    WaterFft fft;
    bool rejected = true;
    for (int resolution : {64, 512, 0})
        rejected = rejected && !SimulateFrame(dev, fft, SettingsFor(resolution), tiles, every, kSimulatedSeconds,
                                              kFrameSeconds) &&
                   *fft.LastFailure() && !fft.Surface(0);
    rejected = rejected && !SimulateFrame(dev, fft, SettingsFor(kWaterFftLowResolution, 0), tiles, every,
                                          kSimulatedSeconds, kFrameSeconds);
    std::vector<WaterFftTile> tooMany(kWaterMaxTiles + 1, kLakeTiles[0]);
    rejected = rejected && !SimulateFrame(dev, fft, SettingsFor(kWaterFftLowResolution), tooMany,
                                          EveryTile(tooMany.size()), kSimulatedSeconds, kFrameSeconds);
    std::printf("     rejected settings report: %s\n", fft.LastFailure());
    Check(rejected, "water simulation rejects resolutions other than 128 and 256, a zero reference and 9 tiles");

    const bool high =
        SimulateFrame(dev, fft, SettingsFor(kWaterFftHighResolution), tiles, every, kSimulatedSeconds, kFrameSeconds);
    const bool highMaps = high && LevelWidth(fft.Surface(0)) == 256 && fft.Surface(0)->GetLevelCount() == 9 &&
                          LevelWidth(fft.Foam(2)) == 256;
    const bool low =
        SimulateFrame(dev, fft, SettingsFor(kWaterFftLowResolution), tiles, every, kSimulatedSeconds, kFrameSeconds);
    const bool lowMaps = low && LevelWidth(fft.Surface(0)) == 128 && fft.Surface(0)->GetLevelCount() == 8 &&
                         LevelWidth(fft.Foam(2)) == 128;
    Check(highMaps && lowMaps, "water maps follow the resolution setting (256 with 9 mips, 128 with 8)");

    WaterFft fresh;
    TileReadback restarted;
    TileReadback firstStep;
    fft.ReleaseDefaultPool();
    const bool released = !fft.Surface(0) && !fft.Foam(0);
    const bool recreated =
        SimulateFrame(dev, fft, SettingsFor(kWaterFftLowResolution), tiles, every, kSimulatedSeconds,
                      kFrameSeconds) &&
        SimulateFrame(dev, fresh, SettingsFor(kWaterFftLowResolution), tiles, every, kSimulatedSeconds,
                      kFrameSeconds) &&
        ReadTile(dev, fft, 1, restarted) && ReadTile(dev, fresh, 1, firstStep);
    Check(released && recreated && SameBits(restarted.foam, firstStep.foam) &&
              SameBits(restarted.surface, firstStep.surface),
          "water maps are released with the default pool and recreated lazily with foam restarting from zero");
}

struct DeviceSnapshot
{
    std::vector<DWORD> renderStates;
    std::vector<DWORD> samplerStates;
    std::vector<IDirect3DBaseTexture9*> textures;
    std::vector<float> vertexFloats;
    std::vector<int> vertexInts;
    std::vector<BOOL> vertexBools;
    std::vector<float> pixelFloats;
    std::vector<int> pixelInts;
    std::vector<BOOL> pixelBools;
    IDirect3DSurface9* depth = nullptr;
    IDirect3DSurface9* extraTargets[kExtraTargets] = {};
    RECT scissor = {};
    float clipPlane[4] = {};
    UINT streamFrequencies[2] = {};
    IDirect3DVertexBuffer9* secondStream = nullptr;
    UINT secondStreamOffset = 0;
    UINT secondStreamStride = 0;
    IDirect3DIndexBuffer9* indices = nullptr;
};

template <typename T>
T* Unreferenced(T* object)
{
    if (object)
        object->Release();
    return object;
}

std::vector<DWORD> CallerSamplerStages()
{
    std::vector<DWORD> stages;
    for (DWORD stage = WaterFftPasses::kSamplerStages; stage < kPixelSamplers; ++stage)
        stages.push_back(stage);
    stages.insert(stages.end(), std::begin(kVertexSamplers), std::end(kVertexSamplers));
    return stages;
}

DeviceSnapshot Snapshot(IDirect3DDevice9* dev)
{
    DeviceSnapshot snapshot;
    for (D3DRENDERSTATETYPE state : kEveryRenderState)
    {
        DWORD value = 0;
        dev->GetRenderState(state, &value);
        snapshot.renderStates.push_back(value);
    }
    for (DWORD stage : CallerSamplerStages())
    {
        for (D3DSAMPLERSTATETYPE state : kEverySamplerState)
        {
            DWORD value = 0;
            dev->GetSamplerState(stage, state, &value);
            snapshot.samplerStates.push_back(value);
        }
        IDirect3DBaseTexture9* texture = nullptr;
        dev->GetTexture(stage, &texture);
        snapshot.textures.push_back(Unreferenced(texture));
    }
    snapshot.vertexFloats.resize(kVertexFloatConstants * 4);
    snapshot.vertexInts.resize(kIntConstants * 4);
    snapshot.vertexBools.resize(kBoolConstants);
    snapshot.pixelFloats.resize((kPixelFloatConstants - kFirstCallerPixelConstant) * 4);
    snapshot.pixelInts.resize(kIntConstants * 4);
    snapshot.pixelBools.resize(kBoolConstants);
    dev->GetVertexShaderConstantF(0, snapshot.vertexFloats.data(), kVertexFloatConstants);
    dev->GetVertexShaderConstantI(0, snapshot.vertexInts.data(), kIntConstants);
    dev->GetVertexShaderConstantB(0, snapshot.vertexBools.data(), kBoolConstants);
    dev->GetPixelShaderConstantF(kFirstCallerPixelConstant, snapshot.pixelFloats.data(),
                                 kPixelFloatConstants - kFirstCallerPixelConstant);
    dev->GetPixelShaderConstantI(0, snapshot.pixelInts.data(), kIntConstants);
    dev->GetPixelShaderConstantB(0, snapshot.pixelBools.data(), kBoolConstants);
    IDirect3DSurface9* depth = nullptr;
    dev->GetDepthStencilSurface(&depth);
    snapshot.depth = Unreferenced(depth);
    for (int index = 0; index < kExtraTargets; ++index)
    {
        IDirect3DSurface9* target = nullptr;
        dev->GetRenderTarget(index + 1, &target);
        snapshot.extraTargets[index] = Unreferenced(target);
    }
    dev->GetScissorRect(&snapshot.scissor);
    dev->GetClipPlane(0, snapshot.clipPlane);
    dev->GetStreamSourceFreq(0, &snapshot.streamFrequencies[0]);
    dev->GetStreamSourceFreq(1, &snapshot.streamFrequencies[1]);
    IDirect3DVertexBuffer9* stream = nullptr;
    dev->GetStreamSource(1, &stream, &snapshot.secondStreamOffset, &snapshot.secondStreamStride);
    snapshot.secondStream = Unreferenced(stream);
    IDirect3DIndexBuffer9* indices = nullptr;
    dev->GetIndices(&indices);
    snapshot.indices = Unreferenced(indices);
    return snapshot;
}

std::string SnapshotDifference(const DeviceSnapshot& before, const DeviceSnapshot& after)
{
    for (size_t i = 0; i < before.renderStates.size(); ++i)
        if (before.renderStates[i] != after.renderStates[i])
            return "render state " + std::to_string(kEveryRenderState[i]);
    if (before.samplerStates != after.samplerStates)
        return "sampler states of stages 4 and up";
    if (before.textures != after.textures)
        return "textures of stages 4 and up";
    if (before.vertexFloats != after.vertexFloats || before.vertexInts != after.vertexInts ||
        before.vertexBools != after.vertexBools)
        return "vertex shader constants";
    if (before.pixelFloats != after.pixelFloats)
        return "pixel shader constants c16 and up";
    if (before.pixelInts != after.pixelInts || before.pixelBools != after.pixelBools)
        return "pixel shader integer or boolean constants";
    if (before.depth != after.depth)
        return "depth-stencil surface";
    if (!std::equal(std::begin(before.extraTargets), std::end(before.extraTargets), std::begin(after.extraTargets)))
        return "render targets 1 to 3";
    if (std::memcmp(&before.scissor, &after.scissor, sizeof(RECT)) != 0)
        return "scissor rectangle";
    if (std::memcmp(before.clipPlane, after.clipPlane, sizeof(before.clipPlane)) != 0)
        return "clip plane";
    if (before.streamFrequencies[0] != after.streamFrequencies[0] ||
        before.streamFrequencies[1] != after.streamFrequencies[1])
        return "stream frequencies";
    if (before.secondStream != after.secondStream || before.secondStreamOffset != after.secondStreamOffset ||
        before.secondStreamStride != after.secondStreamStride)
        return "stream 1";
    if (before.indices != after.indices)
        return "index buffer";
    return std::string();
}

void SetSentinels(IDirect3DDevice9* dev, IDirect3DTexture9* texture, IDirect3DVertexBuffer9* stream)
{
    for (const RenderStateValue& setting : kSentinelRenderStates)
        dev->SetRenderState(setting.state, setting.value);
    std::vector<float> vertexFloats(kVertexFloatConstants * 4);
    for (size_t i = 0; i < vertexFloats.size(); ++i)
        vertexFloats[i] = 0.25f * static_cast<float>(i) + 1.0f;
    dev->SetVertexShaderConstantF(0, vertexFloats.data(), kVertexFloatConstants);
    std::vector<float> pixelFloats((kPixelFloatConstants - kFirstCallerPixelConstant) * 4);
    for (size_t i = 0; i < pixelFloats.size(); ++i)
        pixelFloats[i] = -0.5f * static_cast<float>(i) - 3.0f;
    dev->SetPixelShaderConstantF(kFirstCallerPixelConstant, pixelFloats.data(),
                                 kPixelFloatConstants - kFirstCallerPixelConstant);
    const int integers[4] = {3, 1, 4, 1};
    const BOOL flag = TRUE;
    dev->SetVertexShaderConstantI(2, integers, 1);
    dev->SetPixelShaderConstantI(3, integers, 1);
    dev->SetVertexShaderConstantB(4, &flag, 1);
    dev->SetPixelShaderConstantB(5, &flag, 1);
    dev->SetTexture(kSentinelTextureStage, texture);
    dev->SetSamplerState(kSentinelTextureStage, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR);
    dev->SetSamplerState(kSentinelTextureStage, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    dev->SetSamplerState(kSentinelTextureStage, D3DSAMP_MAXANISOTROPY, 4);
    const RECT scissor = {1, 2, 3, 4};
    dev->SetScissorRect(&scissor);
    const float plane[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    dev->SetClipPlane(0, plane);
    dev->SetStreamSource(1, stream, 16, 16);
}

void CheckStateContract(IDirect3DDevice9* dev)
{
    IDirect3DTexture9* texture = nullptr;
    IDirect3DVertexBuffer9* stream = nullptr;
    const bool created =
        SUCCEEDED(dev->CreateTexture(4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, nullptr)) &&
        SUCCEEDED(dev->CreateVertexBuffer(64, D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &stream, nullptr));
    const std::vector<WaterFftTile> tiles = TileList(kLakeTiles);
    TileReadback results[2];
    bool kept[2] = {};
    for (int run = 0; created && run < 2; ++run)
    {
        const DWORD secondTargetWrites = run == 0 ? kAllChannels : 0;
        WaterFft fft;
        EnterCallerState(dev);
        SetSentinels(dev, texture, stream);
        dev->SetRenderState(D3DRS_COLORWRITEENABLE1, secondTargetWrites);
        const DeviceSnapshot before = Snapshot(dev);
        bool simulated = SUCCEEDED(dev->BeginScene());
        simulated = simulated && fft.Simulate(dev, SettingsFor(kWaterFftLowResolution), tiles,
                                              EveryTile(tiles.size()), kSimulatedSeconds, kFrameSeconds);
        dev->EndScene();
        const DeviceSnapshot after = Snapshot(dev);
        const std::string difference = SnapshotDifference(before, after);
        std::printf("     state contract with COLORWRITEENABLE1 0x%lX: %s\n", secondTargetWrites,
                    difference.empty() ? "unchanged" : difference.c_str());
        kept[run] = simulated && difference.empty() && !after.depth && !after.extraTargets[0];
        kept[run] = ReadTile(dev, fft, 1, results[run]) && kept[run];
    }
    Check(kept[0] && kept[1],
          "water simulation leaves depth-stencil and RT1 unbound and every state outside its contract unchanged");
    Check(created && SameBits(results[0].surface, results[1].surface) && SameBits(results[0].foam, results[1].foam),
          "water surface and foam are identical whether written together (MRT) or in separate passes");
    dev->SetRenderState(D3DRS_COLORWRITEENABLE1, kAllChannels);
    if (texture)
        texture->Release();
    if (stream)
        stream->Release();
}

class WaterStopwatch
{
public:
    ~WaterStopwatch()
    {
        for (IDirect3DQuery9* query : {m_start, m_end, m_frequency, m_disjoint, m_pixels})
            if (query)
                query->Release();
    }

    bool Create(IDirect3DDevice9* dev)
    {
        m_timed = SUCCEEDED(dev->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &m_start)) &&
                  SUCCEEDED(dev->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &m_end)) &&
                  SUCCEEDED(dev->CreateQuery(D3DQUERYTYPE_TIMESTAMPFREQ, &m_frequency)) &&
                  SUCCEEDED(dev->CreateQuery(D3DQUERYTYPE_TIMESTAMPDISJOINT, &m_disjoint));
        return SUCCEEDED(dev->CreateQuery(D3DQUERYTYPE_OCCLUSION, &m_pixels));
    }

    void Start()
    {
        if (m_timed)
        {
            m_disjoint->Issue(D3DISSUE_BEGIN);
            m_start->Issue(D3DISSUE_END);
        }
        m_pixels->Issue(D3DISSUE_BEGIN);
    }

    void Stop()
    {
        m_pixels->Issue(D3DISSUE_END);
        if (m_timed)
        {
            m_end->Issue(D3DISSUE_END);
            m_frequency->Issue(D3DISSUE_END);
            m_disjoint->Issue(D3DISSUE_END);
        }
    }

    bool Read(double& milliseconds, DWORD& pixels)
    {
        milliseconds = -1.0;
        if (!Wait(m_pixels, &pixels, sizeof(pixels)))
            return false;
        UINT64 start = 0;
        UINT64 end = 0;
        UINT64 frequency = 0;
        BOOL disjoint = TRUE;
        if (m_timed && Wait(m_start, &start, sizeof(start)) && Wait(m_end, &end, sizeof(end)) &&
            Wait(m_frequency, &frequency, sizeof(frequency)) && Wait(m_disjoint, &disjoint, sizeof(disjoint)) &&
            !disjoint && frequency && end >= start)
            milliseconds = 1000.0 * static_cast<double>(end - start) / static_cast<double>(frequency);
        return true;
    }

private:
    static bool Wait(IDirect3DQuery9* query, void* data, DWORD size)
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

    IDirect3DQuery9* m_start = nullptr;
    IDirect3DQuery9* m_end = nullptr;
    IDirect3DQuery9* m_frequency = nullptr;
    IDirect3DQuery9* m_disjoint = nullptr;
    IDirect3DQuery9* m_pixels = nullptr;
    bool m_timed = false;
};

struct FramePlan
{
    unsigned draws;
    DWORD pixels;
};

FramePlan PlanFrame(int resolution, int tiles, bool pairedTargets)
{
    const unsigned stages = static_cast<unsigned>(WaterFftStages(resolution));
    const unsigned transformPasses = 2 * ((stages + 1) / 2);
    const DWORD area = static_cast<DWORD>(resolution) * resolution;
    const unsigned passesPerLevel = pairedTargets ? 1u : 2u;
    FramePlan plan = {};
    plan.draws = tiles * (1 + 2 + stages * passesPerLevel) + transformPasses;
    plan.pixels = (1 + transformPasses + 2) * tiles * area;
    for (unsigned level = 1; level <= stages; ++level)
    {
        const DWORD side = static_cast<DWORD>(resolution) >> level;
        plan.pixels += passesPerLevel * tiles * side * side;
    }
    return plan;
}

double Median(std::vector<double> values)
{
    if (values.empty())
        return -1.0;
    std::nth_element(values.begin(), values.begin() + values.size() / 2, values.end());
    return values[values.size() / 2];
}

double MillisecondsBetween(const LARGE_INTEGER& start, const LARGE_INTEGER& end)
{
    LARGE_INTEGER frequency = {};
    QueryPerformanceFrequency(&frequency);
    return 1000.0 * static_cast<double>(end.QuadPart - start.QuadPart) / static_cast<double>(frequency.QuadPart);
}

void WarmUpGpuClocks(IDirect3DDevice9* dev)
{
    const std::vector<WaterFftTile> tiles = TileList(kOceanTiles);
    WaterFft fft;
    IDirect3DQuery9* drained = nullptr;
    if (FAILED(dev->CreateQuery(D3DQUERYTYPE_EVENT, &drained)))
        return;
    double seconds = kSimulatedSeconds;
    const ULONGLONG started = GetTickCount64();
    while (GetTickCount64() - started < kGpuClockWarmupMs)
    {
        SimulateFrame(dev, fft, SettingsFor(kWaterFftHighResolution), tiles, EveryTile(tiles.size()), seconds,
                      static_cast<float>(kTimedFrameSeconds));
        seconds += kTimedFrameSeconds;
        drained->Issue(D3DISSUE_END);
        while (drained->GetData(nullptr, 0, D3DGETDATA_FLUSH) == S_FALSE)
            Sleep(0);
    }
    drained->Release();
}

void ReportGpuTime(IDirect3DDevice9* dev)
{
    D3DCAPS9 caps = {};
    dev->GetDeviceCaps(&caps);
    const bool paired = caps.NumSimultaneousRTs >= 2;
    struct Case
    {
        const char* name;
        std::vector<WaterFftTile> tiles;
    };
    const Case cases[] = {{"lake 32/64/16", TileList(kLakeTiles)}, {"ocean 128/256/512/32", TileList(kOceanTiles)}};
    bool measured = true;
    bool planned = true;
    WarmUpGpuClocks(dev);
    for (int resolution : {kWaterFftLowResolution, kWaterFftHighResolution})
        for (const Case& test : cases)
        {
            WaterFft fft;
            WaterStopwatch stopwatches[kTimedFrames];
            for (WaterStopwatch& stopwatch : stopwatches)
                measured = stopwatch.Create(dev) && measured;
            const WaterFftSettings settings = SettingsFor(resolution);
            const uint32_t every = EveryTile(test.tiles.size());
            double seconds = kSimulatedSeconds;
            std::vector<double> cpu;
            bool simulated = measured;
            for (int frame = 0; simulated && frame < kWarmupFrames + kTimedFrames;
                 ++frame, seconds += kTimedFrameSeconds)
            {
                const int timed = frame - kWarmupFrames;
                EnterCallerState(dev);
                simulated = SUCCEEDED(dev->BeginScene());
                if (!simulated)
                    break;
                if (timed >= 0)
                    stopwatches[timed].Start();
                LARGE_INTEGER before = {};
                LARGE_INTEGER after = {};
                QueryPerformanceCounter(&before);
                simulated = fft.Simulate(dev, settings, test.tiles, every, seconds,
                                         static_cast<float>(kTimedFrameSeconds));
                QueryPerformanceCounter(&after);
                if (timed >= 0)
                {
                    stopwatches[timed].Stop();
                    cpu.push_back(MillisecondsBetween(before, after));
                }
                dev->EndScene();
            }
            std::vector<double> gpu;
            DWORD pixels = 0;
            for (int frame = 0; simulated && frame < kTimedFrames; ++frame)
            {
                double milliseconds = -1.0;
                simulated = stopwatches[frame].Read(milliseconds, pixels);
                if (milliseconds >= 0.0)
                    gpu.push_back(milliseconds);
            }
            measured = measured && simulated;
            const FramePlan plan = PlanFrame(resolution, static_cast<int>(test.tiles.size()), paired);
            planned = planned && pixels == plan.pixels;
            std::printf("     water FFT %-21s N %d: GPU %.3f ms, CPU %.3f ms (medians of %zu frames), %u draws, "
                        "%lu pixels (plan %lu)\n",
                        test.name, resolution, Median(gpu), Median(cpu), gpu.size(), plan.draws, pixels, plan.pixels);
        }
    Check(measured && planned,
          "water FFT frames shade exactly the planned passes (evolve, butterfly stage pairs, surface, mips)");
}

void CheckWaterFft(IDirect3DDevice9* dev)
{
    CheckButterfliesInvertTheDft();
    CheckSpectrumMatchesForeverReference();
    CheckFoldMatchesSeparateTransforms();
    CheckReferenceSingleMode();
    CheckFoamOdeReachesEquilibrium();

    IDirect3DStateBlock9* state = nullptr;
    IDirect3DSurface9* target = nullptr;
    IDirect3DSurface9* depth = nullptr;
    dev->CreateStateBlock(D3DSBT_ALL, &state);
    dev->GetRenderTarget(0, &target);
    dev->GetDepthStencilSurface(&depth);

    WaterFft probe;
    const bool supported = SimulateFrame(dev, probe, SettingsFor(kWaterFftLowResolution), TileList(kLakeTiles),
                                         EveryTile(std::size(kLakeTiles)), kSimulatedSeconds, kFrameSeconds);
    std::printf("     water FFT on this device: %s\n", supported ? "supported" : probe.LastFailure());
    Check(supported, "water FFT simulation runs on the harness device");
    probe.ReleaseAll();
    if (supported)
    {
        CheckGpuButterfliesInvertTheDft(dev);
        CheckGpuSingleMode(dev);
        CheckGpuMatchesReference(dev);
        CheckKitStatistics(dev);
        CheckSimulatedMapsMatchReference(dev);
        CheckStepZeroAndRepeatability(dev);
        CheckStillWater(dev);
        CheckTileMaskKeepsOtherTiles(dev);
        CheckTileSetChangesLogQuietly(dev);
        CheckAmplitudeScaleScalesTheWaves(dev);
        CheckChangedTilesRestart(dev);
        CheckResolutionsAndRecreation(dev);
        CheckStateContract(dev);
        ReportGpuTime(dev);
    }

    if (state)
    {
        state->Apply();
        state->Release();
    }
    if (target)
    {
        dev->SetRenderTarget(0, target);
        target->Release();
    }
    for (int index = 1; index <= kExtraTargets; ++index)
        dev->SetRenderTarget(index, nullptr);
    dev->SetDepthStencilSurface(depth);
    if (depth)
        depth->Release();
}
}
