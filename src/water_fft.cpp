#include "water_fft.h"

WaterFft::~WaterFft()
{
    ReleaseAll();
}

void WaterFft::ReleaseDefaultPool()
{
}

void WaterFft::ReleaseAll()
{
    ReleaseDefaultPool();
}

bool WaterFft::Simulate(IDirect3DDevice9*, const WaterFftSettings&, const std::vector<WaterFftTile>&, uint32_t, double,
                        float)
{
    m_failure = "wave simulation not implemented";
    return false;
}

IDirect3DTexture9* WaterFft::Surface(int) const
{
    return nullptr;
}

IDirect3DTexture9* WaterFft::Foam(int) const
{
    return nullptr;
}
