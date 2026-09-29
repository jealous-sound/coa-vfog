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

class WaterFftPasses
{
public:
    static constexpr int kSamplerStages = 4;

    ~WaterFftPasses();

    void ReleaseDefaultPool();
    void ReleaseAll();

    bool Prepare(IDirect3DDevice9* dev, int resolution, int slots);
    bool CreateSurfaceMap(IDirect3DTexture9** surface) const;
    bool CreateFoamMap(IDirect3DTexture9** foam) const;

    void Begin(IDirect3DDevice9* dev);
    void ClearFoam(IDirect3DTexture9* foam);
    void Evolve(int slot, const WaterFftTile& tile, const WaterFftSettings& settings, double seconds);
    void Transform(int slots, int referenceResolution);
    void UpdateMaps(int slot, const WaterFftTile& tile, float deltaSeconds, IDirect3DTexture9* surface,
                    IDirect3DTexture9* previousFoam, IDirect3DTexture9* nextFoam);
    void End();

    bool Unsupported() const { return m_unsupported; }
    const char* LastFailure() const { return m_failure; }
    int Resolution() const { return m_resolution; }
    int Slots() const { return m_slots; }
    int MaxSlots() const;
    bool PairsTargets() const { return m_pairTargets; }
    IDirect3DTexture9* Spectrum() const { return m_atlas[0]; }
    IDirect3DTexture9* Displacement() const { return m_atlas[m_displacementAtlas]; }
    unsigned Draws() const { return m_draws; }

private:
    enum class PassShader
    {
        Evolve,
        Rows,
        RowsFused,
        Columns,
        ColumnsFused,
        AssembleFused,
        SurfaceAndFoam,
        Surface,
        Foam,
        Mip,
        MipPair,
        Count,
    };

    static constexpr int kPassShaders = static_cast<int>(PassShader::Count);

    static PassShader TransformShader(bool columns, bool fused, bool assemble);

    bool Fail(const char* reason);
    bool Unsupport(const char* reason);
    bool CheckCapabilities(IDirect3DDevice9* dev);
    bool EnsureShaders(IDirect3DDevice9* dev);
    bool EnsureTables(IDirect3DDevice9* dev);
    bool EnsureAtlas(IDirect3DDevice9* dev, int slots);
    bool EnsureMapScratch();
    void ReleaseResolutionResources();
    void UseShader(PassShader shader);
    void BindTexture(DWORD stage, IDirect3DBaseTexture9* texture);
    void BindTarget(DWORD index, IDirect3DTexture9* texture, UINT level);
    void BindTargets(IDirect3DTexture9* first, IDirect3DTexture9* second, UINT level);
    void SetViewport(UINT x, UINT width, UINT height);
    void Draw();
    void UpdatePairedMaps(IDirect3DTexture9* surface, IDirect3DTexture9* nextFoam);
    void UpdateSeparateMaps(IDirect3DTexture9* surface, IDirect3DTexture9* nextFoam);
    void ResampleLevel(PassShader shader, IDirect3DTexture9* source, UINT sourceLevel, IDirect3DTexture9* target,
                       IDirect3DTexture9* pairedTarget, UINT level);

    IDirect3DDevice9* m_device = nullptr;
    IDirect3DDevice9* m_passDevice = nullptr;
    const char* m_failure = "";
    bool m_unsupported = false;
    bool m_capabilitiesChecked = false;
    bool m_multipleTargets = false;
    bool m_independentWriteMasks = false;
    bool m_pairTargets = false;
    UINT m_maxTextureWidth = 0;
    int m_resolution = 0;
    int m_tableResolution = 0;
    int m_slots = 0;
    int m_displacementAtlas = 0;
    unsigned m_draws = 0;
    IDirect3DVertexShader9* m_vs = nullptr;
    IDirect3DVertexDeclaration9* m_decl = nullptr;
    IDirect3DPixelShader9* m_shaders[kPassShaders] = {};
    IDirect3DTexture9* m_noise = nullptr;
    IDirect3DTexture9* m_butterflies = nullptr;
    IDirect3DTexture9* m_atlas[2] = {};
    IDirect3DTexture9* m_mapScratch[2] = {};
    std::vector<float> m_noiseTexels;
    std::vector<float> m_butterflyTexels;
    RECT m_callerScissor = {};
    IDirect3DPixelShader9* m_boundShader = nullptr;
    IDirect3DSurface9* m_boundTargets[2] = {};
    bool m_targetKnown[2] = {};
    IDirect3DBaseTexture9* m_boundTextures[kSamplerStages] = {};
    bool m_samplerKnown[kSamplerStages] = {};
};

class WaterFft
{
public:
    ~WaterFft();

    void ReleaseDefaultPool();
    void ReleaseAll();

    bool Simulate(IDirect3DDevice9* dev, const WaterFftSettings& settings, const std::vector<WaterFftTile>& tiles,
                  uint32_t tileMask, double seconds, float deltaSeconds);
    bool Prepare(IDirect3DDevice9* dev, const WaterFftSettings& settings, const std::vector<WaterFftTile>& tiles,
                 uint32_t tileMask);
    bool Run(IDirect3DDevice9* dev, const WaterFftSettings& settings, const std::vector<WaterFftTile>& tiles,
             double seconds, float deltaSeconds);
    void LogPlan();

    IDirect3DTexture9* Surface(int tile) const;
    IDirect3DTexture9* Foam(int tile) const;
    const char* LastFailure() const { return m_failure; }

private:
    struct TileMaps
    {
        WaterFftTile params = {};
        IDirect3DTexture9* surface = nullptr;
        IDirect3DTexture9* foam[2] = {};
        int latestFoam = 0;
        bool simulated = false;
    };

    bool Fail(const char* reason);
    bool FailCreation(const char* reason);
    void ReleaseTileMaps(TileMaps& maps);
    void ForgetChangedTiles(const std::vector<WaterFftTile>& tiles);
    bool EnsureTileMaps(TileMaps& maps, bool& created);
    void SimulateBatch(const std::vector<WaterFftTile>& tiles, const int* indices, int count,
                       const WaterFftSettings& settings, double seconds, float deltaSeconds);

    const char* m_failure = "";
    const char* m_retryFailure = "";
    unsigned m_framesUntilRetry = 0;
    WaterFftPasses m_passes;
    std::vector<TileMaps> m_tiles;
    int m_mapResolution = 0;
    int m_active[kWaterMaxTiles] = {};
    bool m_created[kWaterMaxTiles] = {};
    int m_activeCount = 0;
    bool m_prepared = false;
    int m_simulatedResolution = 0;
    int m_loggedResolution = 0;
    int m_loggedTiles = 0;
    unsigned m_loggedDraws = 0;
};
