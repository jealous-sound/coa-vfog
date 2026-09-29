#pragma once

#include "water_types.h"

#include <complex>
#include <vector>

constexpr int kWaterFftLowResolution = 128;
constexpr int kWaterFftHighResolution = 256;
constexpr int kWaterFftTexelChannels = 4;
constexpr double kWaterGravity = 9.80665;
constexpr double kWaterForeverInverseGravity = 0.10197161883115768;
constexpr double kWaterTwoPi = 6.2831854820251465;
constexpr double kWaterLoopSeconds = 1024.0;
constexpr double kWaterLoopFractionSteps = 2048.0;
constexpr double kWaterSmallestWaveNumber = 1.0e-3;
constexpr double kWaterFlatWaveNumber = 1.0e-16;
constexpr double kWaterLargestInversePeakLengthSquared = 1.0e30;

using WaterComplex = std::complex<double>;

struct WaterSpectrumShape
{
    double length;
    double amplitude;
    double inversePeakLengthSquared;
    double alignment;
    double wind[2];
};

struct WaterGaussianPair
{
    WaterComplex towards;
    WaterComplex away;
};

struct WaterForeverAmplitude
{
    WaterComplex towards;
    double awayReal;
    float rootRadius;
};

struct WaterButterfly
{
    WaterComplex twiddle;
    int first;
    int second;
};

struct WaterLoopClock
{
    double fraction;
    float coarse;
    float fine;
};

struct WaterSurfaceStencil
{
    double halfTexelsPerUnit;
    int wideRadius;
    double inverseWideBaseline;
};

struct WaterModeField
{
    std::vector<WaterComplex> height;
    std::vector<WaterComplex> x;
    std::vector<WaterComplex> y;
};

struct WaterPackedSpectrum
{
    std::vector<WaterComplex> heightAndX;
    std::vector<WaterComplex> y;
};

struct WaterDisplacement
{
    double x;
    double y;
    double up;
    double residual;
};

struct WaterSurfaceTexel
{
    double slope[2];
    double slopeSquared;
    double wideSlopeSquared;
    double jacobian;
};

bool IsWaterFftResolution(int resolution);
int WaterFftStages(int resolution);
double WaterTileLength(const WaterFftTile& tile);
void NormalizeWaterWind(const float direction[2], double out[2]);
WaterSpectrumShape MakeWaterSpectrumShape(const WaterFftTile& tile, float windSpeed, const float windDirection[2]);
double WaterWaveNumber(int texel, int resolution, double length);
double WaterSpectrumAmplitude(double kx, double ky, const WaterSpectrumShape& shape);
double WaterNoiseHash(double u, double v);
WaterGaussianPair WaterNoiseAt(int x, int y, int resolution);
float WaterRootRadius(int x, int y, int resolution);
std::vector<float> WaterSpectrumNoise(int resolution);
std::vector<WaterButterfly> WaterButterflies(int resolution);
std::vector<float> WaterButterflyTexels(int resolution);
WaterLoopClock MakeWaterLoopClock(double seconds);
float WaterLoopFrequenciesPerRootRadius(double length);
float WaterFrequencyIndex(float rootRadius, float loopFrequenciesPerRootRadius);
WaterSurfaceStencil MakeWaterSurfaceStencil(int resolution, double length);
double WaterFoamStep(double previous, double jacobian, const float params[3], double deltaSeconds);

std::vector<WaterForeverAmplitude> WaterReferenceH0(const std::vector<float>& noise, int resolution,
                                                    const WaterSpectrumShape& shape);
WaterComplex WaterEvolvedMode(const WaterForeverAmplitude& h0, float loopFrequenciesPerRootRadius,
                              double loopFraction);
WaterModeField WaterReferenceModes(const std::vector<WaterForeverAmplitude>& h0, int resolution, double length,
                                   double loopFraction);
WaterPackedSpectrum WaterFoldModes(const WaterModeField& modes, int resolution);
void WaterInverseTransform(std::vector<WaterComplex>& grid, int resolution);
std::vector<WaterDisplacement> WaterAssembleDisplacement(const WaterPackedSpectrum& transformed, int resolution,
                                                         int referenceResolution);
std::vector<WaterDisplacement> WaterReferenceDisplacement(const WaterFftTile& tile, int resolution,
                                                          int referenceResolution, float windSpeed,
                                                          const float windDirection[2], double seconds);
std::vector<WaterSurfaceTexel> WaterReferenceSurface(const std::vector<WaterDisplacement>& displacement,
                                                     int resolution, double length);
