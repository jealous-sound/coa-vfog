#pragma once

#include "water_types.h"

#include <d3d9.h>

#include <cstdint>
#include <vector>

struct WaterFftSettings
{
    int resolution;
    int referenceResolution;
    float windSpeed;
    float windDirection[2];
};

class WaterFft
{
public:
    ~WaterFft();

    void ReleaseDefaultPool();
    void ReleaseAll();

    bool Simulate(IDirect3DDevice9* dev, const WaterFftSettings& settings, const std::vector<WaterFftTile>& tiles,
                  uint32_t tileMask, double seconds, float deltaSeconds);

    IDirect3DTexture9* Surface(int tile) const;
    IDirect3DTexture9* Foam(int tile) const;
    const char* LastFailure() const { return m_failure; }

private:
    const char* m_failure = "";
};
