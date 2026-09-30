#pragma once

#include "water_contacts.h"

#include <d3d9.h>

#include <cstdint>

constexpr float kWaterRippleTexelYards = 0.125f;
constexpr double kWaterRippleStepsPerSecond = 30.0;
constexpr double kWaterRippleStepSeconds = 1.0 / kWaterRippleStepsPerSecond;
constexpr int kMaxWaterRippleStepsPerFrame = 4;
constexpr uint32_t kMaxWaterRippleDisturbances = 32;
constexpr int kWaterRippleTexelsLow = 256;
constexpr int kWaterRippleTexels = 512;
constexpr uint32_t kWaterRippleQuietSteps = 450;

struct WaterRippleSchedule
{
    int steps = 0;
    double stepSeconds[kMaxWaterRippleStepsPerFrame] = {};
    uint32_t dropped = 0;
};

struct WaterRippleStats
{
    int texels = 0;
    bool running = false;
    bool shaded = false;
    uint32_t contacts = 0;
    uint32_t tracks = 0;
    uint64_t steps = 0;
    uint32_t droppedSteps = 0;
    uint32_t restarts = 0;
};

struct WaterRippleWindow
{
    float origin[2] = {};
    float extent = 0.0f;
    int texels = 0;
    float weight = 0.0f;
};

class WaterRipples
{
public:
    ~WaterRipples();

    void ReleaseDefaultPool();
    void ReleaseAll();
    void Restart();

    bool Prepare(IDirect3DDevice9* dev, int texels);
    WaterRippleSchedule Schedule(double seconds);
    void Step(IDirect3DDevice9* dev, const float centre[2], const WaterRippleDisturbance* disturbances,
              uint32_t count);

    bool Running() const { return m_running; }
    bool Visible() const { return m_running && m_mapsCleared; }
    bool HoldsDeviceResources() const { return m_maps[0] || m_maps[1]; }
    int Texels() const { return m_texels; }
    D3DFORMAT Format() const { return m_format; }
    IDirect3DTexture9* Map() const { return m_maps[m_current]; }
    WaterRippleWindow Window(double seconds) const;
    uint64_t StepsRun() const { return m_stepsRun; }
    uint32_t DroppedSteps() const { return m_droppedSteps; }
    const char* LastFailure() const { return m_failure; }

private:
    struct WindowShift
    {
        int x;
        int y;
    };

    bool Fail(const char* reason);
    bool CheckCapabilities(IDirect3DDevice9* dev);
    bool EnsureShaders(IDirect3DDevice9* dev);
    bool EnsureMaps(IDirect3DDevice9* dev, int texels);
    void ClearMaps(IDirect3DDevice9* dev);
    void SetStepState(IDirect3DDevice9* dev);
    WindowShift Recentre(IDirect3DDevice9* dev, const float centre[2]);
    uint32_t UploadDisturbances(IDirect3DDevice9* dev, WindowShift shift, const WaterRippleDisturbance* disturbances,
                                uint32_t count);

    IDirect3DDevice9* m_device = nullptr;
    IDirect3DVertexShader9* m_vs = nullptr;
    IDirect3DVertexDeclaration9* m_decl = nullptr;
    IDirect3DPixelShader9* m_step = nullptr;
    IDirect3DTexture9* m_maps[2] = {};
    D3DFORMAT m_format = D3DFMT_UNKNOWN;
    bool m_capabilitiesChecked = false;
    bool m_unsupported = false;
    const char* m_failure = "";
    int m_texels = 0;
    int m_current = 0;
    bool m_mapsCleared = false;
    int m_origin[2] = {};
    bool m_running = false;
    double m_epoch = 0.0;
    double m_lastScheduled = 0.0;
    int64_t m_stepsDone = 0;
    uint32_t m_quietSteps = 0;
    uint64_t m_stepsRun = 0;
    uint32_t m_droppedSteps = 0;
};
