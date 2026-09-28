#include "water_renderer.h"

WaterRenderer::~WaterRenderer()
{
    ReleaseAll();
}

void WaterRenderer::ReleaseDefaultPool()
{
    m_fft.ReleaseDefaultPool();
    m_armed = false;
}

void WaterRenderer::ReleaseAll()
{
    ReleaseDefaultPool();
    m_fft.ReleaseAll();
}

bool WaterRenderer::Begin(IDirect3DDevice9*, IDirect3DTexture9*, IDirect3DSurface9*, const FrameInputs&,
                          const WaterInputs&, const Config&)
{
    m_skip = "water shading not implemented";
    return false;
}

void WaterRenderer::Tag(IDirect3DDevice9*, WaterClass)
{
}

void WaterRenderer::Untag(IDirect3DDevice9*)
{
}

void WaterRenderer::End(IDirect3DDevice9*, IDirect3DTexture9*, IDirect3DSurface9*)
{
    m_armed = false;
}

void WaterRenderer::Abort(IDirect3DDevice9*)
{
    m_armed = false;
}
