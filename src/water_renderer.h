#pragma once

#include "config.h"
#include "engine.h"
#include "water_fft.h"
#include "water_types.h"

#include <d3d9.h>

#include <cstdint>
#include <string>
#include <vector>

void OverrideWaterSeconds(double seconds);
void DisableWaveSimulation(bool disabled);
void ForcePackedWaterDepth(bool forced);

constexpr int kWaterQualityLevels = 3;
constexpr int kWaterSkyBands = 5;
constexpr int kWaterShadedMaskSlots = 5;
constexpr int kWaterStencilStates = 9;

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
    struct Float4
    {
        float x, y, z, w;
    };

    struct ShadingConstants
    {
        Float4 light;
        Float4 sunColour;
        Float4 ambient;
        Float4 isotropicLight;
        Float4 sky[kWaterSkyBands];
        Float4 stockFogColour;
        Float4 stockFog;
        Float4 absorption;
        Float4 scatteringIntensities;
        Float4 scatteringTop;
        Float4 scatteringBottom;
        Float4 depthFadeFoam;
        Float4 shoreFoam;
        Float4 waveFoam;
        Float4 waveFoamScaling;
        Float4 surfaceResponse;
        Float4 inverseTileSizes;
        Float4 waveControl;
        Float4 foamScroll;
        Float4 depthFoamScroll;
        Float4 depthDecode;
        Float4 maskTints[kWaterShadedMaskSlots * 2];
    };

    struct SavedTargets
    {
        IDirect3DSurface9* colour[4] = {};
        IDirect3DSurface9* depth = nullptr;
        IDirect3DVertexBuffer9* stream = nullptr;
        UINT streamOffset = 0;
        UINT streamStride = 0;
    };

    bool Skip(const char* reason);
    bool EnsureShaders(IDirect3DDevice9* dev);
    bool EnsureStateBlock(IDirect3DDevice9* dev);
    bool EnsureCopies(IDirect3DDevice9* dev, IDirect3DSurface9* target, UINT w, UINT h);
    bool EnsureFlatTexture(IDirect3DDevice9* dev);
    void EnsureMasks(IDirect3DDevice9* dev);
    IDirect3DTexture9* MaskTexture(int32_t index) const;
    bool UsableTargets(const SavedTargets& saved, IDirect3DSurface9* depthSurface, const D3DVIEWPORT9& vp,
                       D3DSURFACE_DESC& depthDesc);
    void SaveTargets(IDirect3DDevice9* dev, SavedTargets& saved);
    void ReleaseTargets(SavedTargets& saved);
    void RestoreTargets(IDirect3DDevice9* dev, SavedTargets& saved);
    void SetPassState(IDirect3DDevice9* dev);
    void CopyLinearDepth(IDirect3DDevice9* dev, IDirect3DTexture9* depthTexture, IDirect3DTexture9* copy);
    bool CopySceneColour(IDirect3DDevice9* dev, IDirect3DSurface9* target, const D3DVIEWPORT9& vp);
    void ClearWaterStencil(IDirect3DDevice9* dev, IDirect3DSurface9* target, IDirect3DSurface9* depthSurface,
                           const D3DVIEWPORT9& vp);
    void ArmStencilWrites(IDirect3DDevice9* dev);
    void RestoreClientStencil(IDirect3DDevice9* dev);
    bool AnyClassDrawn() const;
    uint32_t DrawnTileMask() const;
    bool SimulateWaves(IDirect3DDevice9* dev, double seconds);
    void ShadeClasses(IDirect3DDevice9* dev, IDirect3DSurface9* target, IDirect3DSurface9* depthSurface,
                      double seconds);
    void BindClassTextures(IDirect3DDevice9* dev, const WaterPreset& preset);
    void FillClassConstants(ShadingConstants& c, const WaterPreset& preset, WaterClass waterClass,
                            double seconds) const;
    void DrawFullscreen(IDirect3DDevice9* dev);

    IDirect3DVertexShader9* m_vs = nullptr;
    IDirect3DVertexDeclaration9* m_decl = nullptr;
    IDirect3DPixelShader9* m_depthCopy = nullptr;
    IDirect3DPixelShader9* m_packedDepthCopy = nullptr;
    IDirect3DPixelShader9* m_shade[kWaterQualityLevels] = {};
    IDirect3DDevice9* m_unsupportedShaderDevice = nullptr;
    IDirect3DStateBlock9* m_state = nullptr;
    IDirect3DTexture9* m_sceneColour = nullptr;
    IDirect3DTexture9* m_sceneDepth = nullptr;
    IDirect3DTexture9* m_waterDepth = nullptr;
    IDirect3DTexture9* m_flat = nullptr;
    std::vector<IDirect3DTexture9*> m_masks;
    bool m_masksUploaded = false;
    uint32_t m_maskRevision = 0;
    UINT m_copyW = 0;
    UINT m_copyH = 0;
    bool m_packedDepth = false;
    bool m_packedDepthForcedCopies = false;
    bool m_copyFailed = false;

    FrameInputs m_in = {};
    WaterInputs m_water = {};
    Config m_cfg;
    float m_common[9][4] = {};
    DWORD m_clientStencil[kWaterStencilStates] = {};
    unsigned m_draws[kWaterClassCount] = {};
    bool m_stencilArmed = false;
    bool m_armed = false;
    bool m_wavesSimulated = false;
    int m_shadedClasses = 0;
    double m_lastSeconds = -1.0;
    std::string m_loggedWaveState;
    bool m_waveStateLogged = false;
    bool m_loggedFirstShade = false;
    const char* m_skip = "";
    WaterFft m_fft;
};
