#pragma once

#include "fog_volume.h"
#include "shader_instrumentation.h"

#include <memory>
#include <string>
#include <vector>

struct FixedFunctionMaterialState;

class MaterialFog
{
public:
    MaterialFog();
    ~MaterialFog();

    bool Begin(const MaterialFogVolume& volume);
    void End();
    void Release();
    void SetRequested(bool requested);
    bool Apply(IDirect3DDevice9* device);
    void Restore();
    bool Compatible() const { return m_compatible; }
    const char* FailureReason() const { return m_failure.c_str(); }

private:
    struct ShaderPair;
    struct SavedDraw;

    ShaderPair* FindOrCreatePair(IDirect3DDevice9* device, IDirect3DVertexShader9* vertex,
                                IDirect3DPixelShader9* pixel, const FixedFunctionMaterialState& fixed);
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
    bool m_requested = true;
    std::string m_failure;
};
