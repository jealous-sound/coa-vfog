#include "water_spectrum.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace
{
constexpr double kHashDotX = 12.9898;
constexpr double kHashDotY = 78.233;
constexpr double kHashScale = 43758.546875;
constexpr double kSmallestHash = 1.0e-16;
constexpr double kTowardsRadiusOffset = 0.13;
constexpr double kTowardsAngleOffset = 0.07;
constexpr double kAwayRadiusOffset = 0.29;
constexpr double kAwayAngleOffset = 0.19;
constexpr double kExactTwoPi = 6.283185307179586;
constexpr double kSmallestWindLength = 1.0e-6;

double Fract(double value)
{
    return value - std::floor(value);
}

WaterComplex GaussianSample(double u, double v, double radiusOffset, double angleOffset)
{
    const double radius = std::sqrt(-2.0 * std::log(WaterNoiseHash(u + radiusOffset, v + radiusOffset)));
    const double angle = kWaterTwoPi * WaterNoiseHash(u + angleOffset, v + angleOffset);
    return {radius * std::cos(angle), radius * std::sin(angle)};
}

int BitReversed(int value, int bits)
{
    int reversed = 0;
    for (int bit = 0; bit < bits; ++bit)
    {
        reversed = (reversed << 1) | (value & 1);
        value >>= 1;
    }
    return reversed;
}

size_t TexelIndex(int x, int y, int resolution)
{
    return static_cast<size_t>(y) * resolution + x;
}

int Wrapped(int texel, int resolution)
{
    return (texel % resolution + resolution) % resolution;
}

int Mirrored(int texel, int resolution)
{
    return (resolution - texel) % resolution;
}

void TransformLine(std::vector<WaterComplex>& line, std::vector<WaterComplex>& scratch,
                   const std::vector<WaterButterfly>& butterflies, int resolution)
{
    const int stages = WaterFftStages(resolution);
    for (int stage = 0; stage < stages; ++stage)
    {
        const WaterButterfly* row = &butterflies[static_cast<size_t>(stage) * resolution];
        for (int i = 0; i < resolution; ++i)
            scratch[i] = line[row[i].first] + row[i].twiddle * line[row[i].second];
        line.swap(scratch);
    }
}

struct DisplacementDerivative
{
    double x;
    double y;
    double up;
};

DisplacementDerivative CentralDifference(const WaterDisplacement& ahead, const WaterDisplacement& behind,
                                         double scale)
{
    return {(ahead.x - behind.x) * scale, (ahead.y - behind.y) * scale, (ahead.up - behind.up) * scale};
}
}

bool IsWaterFftResolution(int resolution)
{
    return resolution == kWaterFftLowResolution || resolution == kWaterFftHighResolution;
}

int WaterFftStages(int resolution)
{
    int stages = 0;
    while ((1 << stages) < resolution)
        ++stages;
    return stages;
}

double WaterTileLength(const WaterFftTile& tile)
{
    return std::max(1.0, std::floor(static_cast<double>(tile.size)));
}

void NormalizeWaterWind(const float direction[2], double out[2])
{
    const double x = direction[0];
    const double y = direction[1];
    const double length = std::sqrt(x * x + y * y);
    if (!(length > kSmallestWindLength) || !std::isfinite(length))
    {
        out[0] = 1.0;
        out[1] = 0.0;
        return;
    }
    out[0] = x / length;
    out[1] = y / length;
}

WaterSpectrumShape MakeWaterSpectrumShape(const WaterFftTile& tile, float windSpeed, const float windDirection[2])
{
    WaterSpectrumShape shape = {};
    shape.length = WaterTileLength(tile);
    shape.amplitude = tile.amplitude;
    const double wind = static_cast<double>(tile.windMultiplier) * windSpeed;
    const double peakLength = wind * wind * kWaterForeverInverseGravity;
    shape.inversePeakLengthSquared = peakLength > 0.0
                                         ? std::min(1.0 / (peakLength * peakLength),
                                                    kWaterLargestInversePeakLengthSquared)
                                         : kWaterLargestInversePeakLengthSquared;
    shape.alignment = tile.windAlignment;
    NormalizeWaterWind(windDirection, shape.wind);
    return shape;
}

double WaterWaveNumber(int texel, int resolution, double length)
{
    return kWaterTwoPi * (texel - 0.5 * resolution) / length;
}

double WaterSpectrumAmplitude(double kx, double ky, const WaterSpectrumShape& shape)
{
    const double squared = kx * kx + ky * ky;
    const double magnitude = std::sqrt(squared);
    if (magnitude < kWaterSmallestWaveNumber)
        return 0.0;
    const double alongWind = (kx * shape.wind[0] + ky * shape.wind[1]) / magnitude;
    if (!(alongWind > 0.0))
        return 0.0;
    const double power = shape.amplitude / (squared * squared) *
                         std::exp(-shape.inversePeakLengthSquared / squared) * std::pow(alongWind, shape.alignment);
    return std::sqrt(power) / std::sqrt(2.0);
}

double WaterNoiseHash(double u, double v)
{
    const double hash = Fract(std::sin(u * kHashDotX + v * kHashDotY) * kHashScale);
    return std::min(std::max(hash, kSmallestHash), 1.0);
}

float WaterRootRadius(int x, int y, int resolution)
{
    const double centredX = x - 0.5 * resolution;
    const double centredY = y - 0.5 * resolution;
    return static_cast<float>(std::sqrt(std::sqrt(centredX * centredX + centredY * centredY)));
}

WaterGaussianPair WaterNoiseAt(int x, int y, int resolution)
{
    const double u = static_cast<double>(x) / resolution;
    const double v = static_cast<double>(y) / resolution;
    return {GaussianSample(u, v, kTowardsRadiusOffset, kTowardsAngleOffset),
            GaussianSample(u, v, kAwayRadiusOffset, kAwayAngleOffset)};
}

std::vector<float> WaterSpectrumNoise(int resolution)
{
    std::vector<float> texels(static_cast<size_t>(resolution) * resolution * kWaterFftTexelChannels);
    for (int y = 0; y < resolution; ++y)
        for (int x = 0; x < resolution; ++x)
        {
            const WaterGaussianPair pair = WaterNoiseAt(x, y, resolution);
            float* texel = &texels[TexelIndex(x, y, resolution) * kWaterFftTexelChannels];
            texel[0] = static_cast<float>(pair.towards.real());
            texel[1] = static_cast<float>(pair.towards.imag());
            texel[2] = static_cast<float>(pair.away.real());
            texel[3] = WaterRootRadius(x, y, resolution);
        }
    return texels;
}

std::vector<WaterButterfly> WaterButterflies(int resolution)
{
    const int stages = WaterFftStages(resolution);
    std::vector<int> reversed(resolution);
    for (int i = 0; i < resolution; ++i)
        reversed[i] = BitReversed(i, stages);
    std::vector<WaterButterfly> butterflies(static_cast<size_t>(stages) * resolution);
    for (int stage = 0; stage < stages; ++stage)
    {
        const int group = 1 << (stage + 1);
        const int span = 1 << stage;
        for (int row = 0; row < resolution; ++row)
        {
            const int twiddleIndex = (row * (resolution / group)) % resolution;
            const double angle = kExactTwoPi * twiddleIndex / resolution;
            const bool top = row % group < span;
            WaterButterfly& butterfly = butterflies[static_cast<size_t>(stage) * resolution + row];
            butterfly.twiddle = WaterComplex(std::cos(angle), std::sin(angle));
            if (stage == 0)
            {
                butterfly.first = top ? reversed[row] : reversed[row - 1];
                butterfly.second = top ? reversed[row + 1] : reversed[row];
            }
            else
            {
                butterfly.first = top ? row : row - span;
                butterfly.second = top ? row + span : row;
            }
        }
    }
    return butterflies;
}

std::vector<float> WaterButterflyTexels(int resolution)
{
    const int stages = WaterFftStages(resolution);
    const std::vector<WaterButterfly> butterflies = WaterButterflies(resolution);
    std::vector<float> texels(butterflies.size() * kWaterFftTexelChannels);
    for (int stage = 0; stage < stages; ++stage)
        for (int row = 0; row < resolution; ++row)
        {
            const WaterButterfly& butterfly = butterflies[static_cast<size_t>(stage) * resolution + row];
            float* texel = &texels[TexelIndex(stage, row, stages) * kWaterFftTexelChannels];
            texel[0] = static_cast<float>(butterfly.twiddle.real());
            texel[1] = static_cast<float>(butterfly.twiddle.imag());
            texel[2] = static_cast<float>((butterfly.first + 0.5) / resolution);
            texel[3] = static_cast<float>((butterfly.second + 0.5) / resolution);
        }
    return texels;
}

WaterLoopClock MakeWaterLoopClock(double seconds)
{
    const double loops = std::isfinite(seconds) ? seconds / kWaterLoopSeconds : 0.0;
    double fraction = Fract(loops);
    if (!(fraction < 1.0))
        fraction = 0.0;
    const double coarse = std::floor(fraction * kWaterLoopFractionSteps) / kWaterLoopFractionSteps;
    return {fraction, static_cast<float>(coarse), static_cast<float>(fraction - coarse)};
}

float WaterLoopFrequenciesPerRootRadius(double length)
{
    return static_cast<float>(kWaterLoopSeconds * std::sqrt(kWaterGravity / (kWaterTwoPi * length)));
}

float WaterFrequencyIndex(float rootRadius, float loopFrequenciesPerRootRadius)
{
    const float loopFrequencies = rootRadius * loopFrequenciesPerRootRadius;
    return std::floor(loopFrequencies + 0.5f);
}

WaterSurfaceStencil MakeWaterSurfaceStencil(int resolution, double length)
{
    const double texelsPerUnit = resolution / length;
    const int wideRadius =
        std::min(std::max(static_cast<int>(std::lround(2.0 * texelsPerUnit)), 1), resolution / 2);
    return {0.5 * texelsPerUnit, wideRadius, texelsPerUnit / (2.0 * wideRadius)};
}

double WaterFoamStep(double previous, double jacobian, const float params[3], double deltaSeconds)
{
    const double foam = std::min(std::max(previous, 0.0), 1.0);
    const double injection = std::max(params[0] + 1.0 - jacobian, 0.0);
    const double next = foam + deltaSeconds * (params[1] * injection * (1.0 - foam) - params[2] * foam);
    return std::min(std::max(next, 0.0), 1.0);
}

std::vector<WaterForeverAmplitude> WaterReferenceH0(const std::vector<float>& noise, int resolution,
                                                    const WaterSpectrumShape& shape)
{
    std::vector<WaterForeverAmplitude> h0(static_cast<size_t>(resolution) * resolution);
    for (int y = 0; y < resolution; ++y)
        for (int x = 0; x < resolution; ++x)
        {
            const double kx = WaterWaveNumber(x, resolution, shape.length);
            const double ky = WaterWaveNumber(y, resolution, shape.length);
            const size_t index = TexelIndex(x, y, resolution);
            const float* texel = &noise[index * kWaterFftTexelChannels];
            h0[index].towards = WaterComplex(texel[0], texel[1]) * WaterSpectrumAmplitude(kx, ky, shape);
            h0[index].awayReal = texel[2] * WaterSpectrumAmplitude(-kx, -ky, shape);
            h0[index].rootRadius = texel[3];
        }
    return h0;
}

WaterComplex WaterEvolvedMode(const WaterForeverAmplitude& h0, float loopFrequenciesPerRootRadius,
                              double loopFraction)
{
    const double loopFrequency = WaterFrequencyIndex(h0.rootRadius, loopFrequenciesPerRootRadius);
    const double phase = kWaterTwoPi * Fract(loopFrequency * loopFraction);
    const WaterComplex rotation(std::cos(phase), std::sin(phase));
    const WaterComplex foreverAway(h0.awayReal, h0.towards.imag());
    return h0.towards * rotation + std::conj(foreverAway) * std::conj(rotation);
}

WaterModeField WaterReferenceModes(const std::vector<WaterForeverAmplitude>& h0, int resolution, double length,
                                   double loopFraction)
{
    const float loopFrequenciesPerRootRadius = WaterLoopFrequenciesPerRootRadius(length);
    const size_t texels = static_cast<size_t>(resolution) * resolution;
    WaterModeField modes = {std::vector<WaterComplex>(texels), std::vector<WaterComplex>(texels),
                            std::vector<WaterComplex>(texels)};
    for (int y = 0; y < resolution; ++y)
        for (int x = 0; x < resolution; ++x)
        {
            const double kx = WaterWaveNumber(x, resolution, length);
            const double ky = WaterWaveNumber(y, resolution, length);
            const double magnitude = std::sqrt(kx * kx + ky * ky);
            const double divisor = std::max(magnitude, kWaterFlatWaveNumber);
            const size_t index = TexelIndex(x, y, resolution);
            const WaterComplex height = WaterEvolvedMode(h0[index], loopFrequenciesPerRootRadius, loopFraction);
            modes.height[index] = height;
            modes.x[index] = WaterComplex(0.0, -kx / divisor) * height;
            modes.y[index] = WaterComplex(0.0, -ky / divisor) * height;
        }
    return modes;
}

WaterPackedSpectrum WaterFoldModes(const WaterModeField& modes, int resolution)
{
    const size_t texels = static_cast<size_t>(resolution) * resolution;
    WaterPackedSpectrum packed = {std::vector<WaterComplex>(texels), std::vector<WaterComplex>(texels)};
    const WaterComplex i(0.0, 1.0);
    for (int y = 0; y < resolution; ++y)
        for (int x = 0; x < resolution; ++x)
        {
            const size_t at = TexelIndex(x, y, resolution);
            const size_t mirror = TexelIndex(Mirrored(x, resolution), Mirrored(y, resolution), resolution);
            const WaterComplex height = 0.5 * (modes.height[at] + std::conj(modes.height[mirror]));
            const WaterComplex alongX = 0.5 * (modes.x[at] + std::conj(modes.x[mirror]));
            const WaterComplex alongY = 0.5 * (modes.y[at] + std::conj(modes.y[mirror]));
            packed.heightAndX[at] = height + i * alongX;
            packed.y[at] = alongY;
        }
    return packed;
}

void WaterInverseTransform(std::vector<WaterComplex>& grid, int resolution)
{
    const std::vector<WaterButterfly> butterflies = WaterButterflies(resolution);
    std::vector<WaterComplex> line(resolution);
    std::vector<WaterComplex> scratch(resolution);
    for (int y = 0; y < resolution; ++y)
    {
        for (int x = 0; x < resolution; ++x)
            line[x] = grid[TexelIndex(x, y, resolution)];
        TransformLine(line, scratch, butterflies, resolution);
        for (int x = 0; x < resolution; ++x)
            grid[TexelIndex(x, y, resolution)] = line[x];
    }
    for (int x = 0; x < resolution; ++x)
    {
        for (int y = 0; y < resolution; ++y)
            line[y] = grid[TexelIndex(x, y, resolution)];
        TransformLine(line, scratch, butterflies, resolution);
        for (int y = 0; y < resolution; ++y)
            grid[TexelIndex(x, y, resolution)] = line[y];
    }
}

std::vector<WaterDisplacement> WaterAssembleDisplacement(const WaterPackedSpectrum& transformed, int resolution,
                                                         int referenceResolution)
{
    const double area = static_cast<double>(referenceResolution) * referenceResolution;
    std::vector<WaterDisplacement> displacement(static_cast<size_t>(resolution) * resolution);
    for (int y = 0; y < resolution; ++y)
        for (int x = 0; x < resolution; ++x)
        {
            const size_t index = TexelIndex(x, y, resolution);
            const double scale = ((x + y) & 1 ? -1.0 : 1.0) / area;
            const WaterComplex heightAndX = transformed.heightAndX[index];
            const WaterComplex alongY = transformed.y[index];
            displacement[index] = {scale * heightAndX.imag(), scale * alongY.real(), -scale * heightAndX.real(),
                                   scale * alongY.imag()};
        }
    return displacement;
}

std::vector<WaterDisplacement> WaterReferenceDisplacement(const WaterFftTile& tile, int resolution,
                                                          int referenceResolution, float windSpeed,
                                                          const float windDirection[2], double seconds)
{
    const WaterSpectrumShape shape = MakeWaterSpectrumShape(tile, windSpeed, windDirection);
    const std::vector<WaterForeverAmplitude> h0 =
        WaterReferenceH0(WaterSpectrumNoise(resolution), resolution, shape);
    const WaterModeField modes =
        WaterReferenceModes(h0, resolution, shape.length, MakeWaterLoopClock(seconds).fraction);
    WaterPackedSpectrum packed = WaterFoldModes(modes, resolution);
    WaterInverseTransform(packed.heightAndX, resolution);
    WaterInverseTransform(packed.y, resolution);
    return WaterAssembleDisplacement(packed, resolution, referenceResolution);
}

std::vector<WaterSurfaceTexel> WaterReferenceSurface(const std::vector<WaterDisplacement>& displacement,
                                                     int resolution, double length)
{
    const WaterSurfaceStencil stencil = MakeWaterSurfaceStencil(resolution, length);
    const int wide = stencil.wideRadius;
    std::vector<WaterSurfaceTexel> surface(displacement.size());
    for (int y = 0; y < resolution; ++y)
        for (int x = 0; x < resolution; ++x)
        {
            auto at = [&](int dx, int dy) -> const WaterDisplacement& {
                return displacement[TexelIndex(Wrapped(x + dx, resolution), Wrapped(y + dy, resolution),
                                               resolution)];
            };
            const DisplacementDerivative alongX = CentralDifference(at(1, 0), at(-1, 0), stencil.halfTexelsPerUnit);
            const DisplacementDerivative alongY = CentralDifference(at(0, 1), at(0, -1), stencil.halfTexelsPerUnit);
            const double wideX = (at(wide, 0).up - at(-wide, 0).up) * stencil.inverseWideBaseline;
            const double wideY = (at(0, wide).up - at(0, -wide).up) * stencil.inverseWideBaseline;
            WaterSurfaceTexel& texel = surface[TexelIndex(x, y, resolution)];
            texel.slope[0] = alongX.up;
            texel.slope[1] = alongY.up;
            texel.slopeSquared = alongX.up * alongX.up + alongY.up * alongY.up;
            texel.wideSlopeSquared = wideX * wideX + wideY * wideY;
            texel.jacobian = (1.0 + alongX.x) * (1.0 + alongY.y) - alongY.x * alongX.y;
        }
    return surface;
}
