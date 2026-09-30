#pragma once

#include "config.h"
#include "engine.h"
#include "fog_model.h"
#include "gpu_timing.h"
#include "msaa_depth.h"
#include "water_contacts.h"
#include "water_data.h"
#include "water_fft.h"
#include "water_ripples.h"
#include "water_types.h"

#include <d3d9.h>

#include <cstdint>
#include <string>
#include <vector>

void OverrideWaterSeconds(double seconds);
double WaterClockSeconds();
void DisableWaveSimulation(bool disabled);
void ForcePackedWaterDepth(bool forced);

enum class WaterFaultStage
{
    None,
    Begin,
    End,
};

void InjectWaterFault(WaterFaultStage stage);
void FailWaterMaskUploads(int count);
void ForceWaterShadingVariant(int variant);
void ForceWaterSummary();

constexpr int kWaterQualityLevels = 3;
constexpr int kWaterSkyBands = 5;
constexpr int kWaterShadedMaskSlots = 5;
constexpr int kWaterStencilStates = 9;
constexpr unsigned kWaterSceneCopiesHeld = 0x1;
constexpr unsigned kWaterWaveMapsHeld = 0x2;
constexpr unsigned kWaterFoamMasksHeld = 0x4;
constexpr unsigned kWaterRippleMapsHeld = 0x8;

class WaterRenderer
{
public:
    ~WaterRenderer();

    void ReleaseDefaultPool();
    void ReleaseAll();

    bool Begin(IDirect3DDevice9* dev, const SceneDepth& depth, const FrameInputs& in, const WaterInputs& water,
               const Config& cfg);
    void Tag(IDirect3DDevice9* dev, WaterClass waterClass);
    void Untag(IDirect3DDevice9* dev);
    bool End(IDirect3DDevice9* dev, const SceneDepth& depth);
    void Abort(IDirect3DDevice9* dev);

    bool Armed() const { return m_armed; }
    bool WavesSimulated() const { return m_wavesSimulated; }
    unsigned ShadedClasses() const { return m_shadedClassMask; }
    unsigned HeldResources() const;
    int FoamMaskPool() const;
    int UploadedMasks() const;
    int RequiredMasks() const { return static_cast<int>(m_foamMasks.size()); }
    WaterRippleStats RippleStats() const;
    WaterRippleShading RippleShading() const;
    bool RipplesAvailable() const { return m_ripplesAvailable; }
    int LastShadingVariant() const { return m_shadingVariant; }
    const char* LastSkipReason() const { return m_skip; }

private:
    struct Float4
    {
        float x, y, z, w;
    };

    struct ReflectionFogLayer
    {
        Float4 curve;
        Float4 height;
        Float4 emissive;
        Float4 diffuse;
        Float4 scattering;
    };

    struct ReflectionFog
    {
        ReflectionFogLayer layers[kFogLayers];
        Float4 range;
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
        ReflectionFog reflectionFog;
        Float4 rippleWindow;
        Float4 rippleShape;
        Float4 rippleFade;
    };

    struct FoamMaskTexture
    {
        int32_t sources[kWaveFoamMaskSlots];
        bool packed;
        bool present[kWaveFoamMaskSlots];
        IDirect3DTexture9* texture;
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
    void PlanFoamMasks(const WaterData& data);
    void EnsureMasks(IDirect3DDevice9* dev);
    void ReleaseMasks();
    const FoamMaskTexture* SingleMask(int32_t index) const;
    const FoamMaskTexture* WaveFoamMasks(const WaterPreset& preset) const;
    IDirect3DTexture9* MaskTexture(int32_t index) const;
    bool MaskPresent(const WaterPreset& preset, int slot) const;
    bool UsableTargets(IDirect3DSurface9* depthSurface, const D3DVIEWPORT9& vp, D3DSURFACE_DESC& depthDesc);
    void SaveTargets(IDirect3DDevice9* dev);
    void ReleaseTargets();
    void CaptureClientState();
    void RestoreTargets(IDirect3DDevice9* dev);
    void SetPassState(IDirect3DDevice9* dev);
    void CopyLinearDepth(IDirect3DDevice9* dev, IDirect3DTexture9* depthTexture, IDirect3DTexture9* copy);
    bool CopySceneColour(IDirect3DDevice9* dev, IDirect3DSurface9* target, const D3DVIEWPORT9& vp);
    void ClearWaterStencil(IDirect3DDevice9* dev, IDirect3DSurface9* target, IDirect3DSurface9* depthSurface,
                           const D3DVIEWPORT9& vp);
    void ArmStencilWrites(IDirect3DDevice9* dev);
    void RestoreClientStencil(IDirect3DDevice9* dev);
    bool AnyClassDrawn() const;
    uint32_t DrawnTileMask() const;
    bool PrepareWaves(IDirect3DDevice9* dev);
    bool SimulateWaves(IDirect3DDevice9* dev, double seconds);
    bool RippleContinuityBroken(double seconds) const;
    void RestartRipples();
    void ReleaseRipples();
    bool SimulateRipples(IDirect3DDevice9* dev, double seconds);
    void FillRippleConstants(double seconds);
    void LogWaveState();
    void ShadeClasses(IDirect3DDevice9* dev, IDirect3DSurface9* target, IDirect3DSurface9* depthSurface,
                      double seconds);
    void BindClassTextures(IDirect3DDevice9* dev, const WaterPreset& preset);
    void FillClassConstants(ShadingConstants& c, const WaterPreset& preset, WaterClass waterClass,
                            double seconds) const;
    bool ShadeTaggedWater(IDirect3DDevice9* dev, const SceneDepth& depth);
    void BuildReflectionFog();
    void AddToSummary(bool shaded);
    void LogSummaryWhenDue(IDirect3DDevice9* dev);
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
    std::vector<FoamMaskTexture> m_foamMasks;
    bool m_masksPlanned = false;
    bool m_masksUploaded = false;
    int m_maskRetryPasses = 0;
    uint32_t m_maskRevision = 0;
    uint32_t m_loggedMaskRevision = 0;
    bool m_masksLogged = false;
    int m_loggedMaskUploads = 0;
    UINT m_copyW = 0;
    UINT m_copyH = 0;
    bool m_packedDepth = false;
    bool m_packedDepthForcedCopies = false;
    bool m_copyFailed = false;
    UINT m_loggedCopyW = 0;
    UINT m_loggedCopyH = 0;
    bool m_loggedPackedDepth = false;

    SavedTargets m_saved;
    bool m_stateCaptured = false;
    FrameInputs m_in = {};
    WaterInputs m_water = {};
    Config m_cfg;
    float m_common[9][4] = {};
    ReflectionFog m_reflectionFog = {};
    DWORD m_clientStencil[kWaterStencilStates] = {};
    unsigned m_draws[kWaterClassCount] = {};
    bool m_stencilArmed = false;
    bool m_armed = false;
    bool m_wavesSimulated = false;
    bool m_wavesAttempted = false;
    uint32_t m_waveTiles = 0;
    WaterFftSettings m_waveSettings = {};
    int m_shadingVariant = -1;
    int m_shadedClasses = 0;
    unsigned m_shadedClassMask = 0;
    GpuTimer m_gpuTimer{"water"};
    double m_summaryStart = -1.0;
    unsigned m_summaryClasses = 0;
    int m_summaryWaveResolution = 0;
    int m_summaryWaveTiles = 0;
    int m_summaryRippleTexels = 0;
    uint32_t m_summaryContacts = 0;
    uint32_t m_summaryDroppedSteps = 0;
    double m_lastSeconds = -1.0;
    WaterRipples m_ripples;
    WaterContactTracker m_contacts;
    Float4 m_rippleWindow = {};
    Float4 m_rippleShape = {};
    Float4 m_rippleFade = {};
    int m_rippleMapId = 0;
    double m_lastRippleSeconds = -1.0;
    uint32_t m_rippleRestarts = 0;
    bool m_ripplesShaded = false;
    bool m_ripplesAvailable = true;
    const char* m_loggedRippleFailure = "";
    const char* m_loggedWaveState = "";
    bool m_waveStateLogged = false;
    bool m_loggedFirstShade = false;
    const char* m_skip = "";
    WaterFft m_fft;
};
