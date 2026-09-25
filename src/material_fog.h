#pragma once

#include "fog_volume.h"
#include "shader_instrumentation.h"

#include <memory>
#include <vector>

class MaterialFog
{
public:
    MaterialFog();
    ~MaterialFog();

    bool Begin(const MaterialFogVolume& volume);
    void End();
    void Release();
    bool Apply(IDirect3DDevice9* device);
    void Restore();
    bool Compatible() const { return m_compatible; }

private:
    struct ShaderPair;
    struct SavedDraw;

    ShaderPair* FindOrCreatePair(IDirect3DDevice9* device, IDirect3DVertexShader9* vertex,
                                IDirect3DPixelShader9* pixel);
    bool MatchesWorldDraw(IDirect3DDevice9* device) const;
    bool ReadBlendMode(IDirect3DDevice9* device, float& premultiplied, float& additive);
    void LogFallback(const char* reason);

    MaterialFogVolume m_volume;
    std::vector<std::unique_ptr<ShaderPair>> m_pairs;
    std::unique_ptr<SavedDraw> m_saved;
    unsigned long long m_serial = 0;
    unsigned m_fallbacksLogged = 0;
    unsigned m_appliedDraws = 0;
    unsigned m_unsupportedDraws = 0;
    bool m_compatible = true;
};
