#pragma once

#include "config.h"
#include "engine.h"
#include "water_fft.h"
#include "water_types.h"

#include <d3d9.h>

class WaterRenderer
{
public:
    ~WaterRenderer();

    void ReleaseDefaultPool();
    void ReleaseAll();

    bool Begin(IDirect3DDevice9* dev, IDirect3DTexture9* depthTexture, IDirect3DSurface9* depthSurface,
               const FrameInputs& in, const WaterInputs& water, const Config& cfg);
    void Tag(IDirect3DDevice9* dev, WaterClass waterClass);
    void Untag(IDirect3DDevice9* dev);
    void End(IDirect3DDevice9* dev, IDirect3DTexture9* depthTexture, IDirect3DSurface9* depthSurface);
    void Abort(IDirect3DDevice9* dev);

    bool Armed() const { return m_armed; }
    const char* LastSkipReason() const { return m_skip; }

private:
    bool m_armed = false;
    const char* m_skip = "";
    WaterFft m_fft;
};
