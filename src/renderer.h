#pragma once

#include "config.h"
#include "engine.h"
#include "fog_data.h"
#include "fog_volume.h"

#include <d3d9.h>

class Renderer
{
public:
    ~Renderer();

    void ReleaseDefaultPool();
    void ReleaseAll();

    bool Render(IDirect3DDevice9* dev, IDirect3DTexture9* depthTexture, IDirect3DSurface9* boundDepthStencil,
                const FrameInputs& in, const Config& cfg);

    const char* LastSkipReason() const { return m_skip; }
    const MaterialFogVolume& MaterialVolume() const { return m_materialVolume; }
    bool AdaptiveLightingHistory() const { return m_adaptiveLightingHistory; }

private:
    bool EnsureShaders(IDirect3DDevice9* dev);
    bool EnsureStateBlock(IDirect3DDevice9* dev);
    bool EnsureTargets(IDirect3DDevice9* dev, UINT lowW, UINT lowH, UINT rayW, UINT rayH);
    bool EnsureSceneCopy(IDirect3DDevice9* dev, IDirect3DSurface9* target, UINT w, UINT h);
    bool CopyWorldViewport(IDirect3DDevice9* dev, IDirect3DSurface9* target, const D3DVIEWPORT9& vp);
    bool Skip(const char* reason);
    void LogLightChange(const FrameInputs& in, const AuthoredFog& fog, bool authored);
    bool DepthProbeDue(long long now) const;
    void LogDepthProbe(IDirect3DDevice9* dev, IDirect3DTexture9* depthTexture, IDirect3DTexture9* fog,
                       const D3DVIEWPORT9& vp, float deepestWorldDepth, float dayFraction);
    void DrawFullscreen(IDirect3DDevice9* dev);
    void BindTexture(IDirect3DDevice9* dev, DWORD stage, IDirect3DBaseTexture9* tex, bool linear);
    bool RenderPasses(IDirect3DDevice9* dev, IDirect3DTexture9* depthTexture, IDirect3DSurface9* target,
                      const D3DSURFACE_DESC& depthDesc, const FrameInputs& in, const Config& cfg);

    IDirect3DVertexShader9* m_vs = nullptr;
    IDirect3DDevice9* m_unsupportedShaderDevice = nullptr;
    IDirect3DPixelShader9* m_march[3] = {};
    IDirect3DPixelShader9* m_atlasShader[3] = {};
    IDirect3DPixelShader9* m_atlasPrefix = nullptr;
    IDirect3DPixelShader9* m_temporal = nullptr;
    IDirect3DPixelShader9* m_historyDepthShader = nullptr;
    IDirect3DPixelShader9* m_composite[3] = {};
    IDirect3DPixelShader9* m_rayMask = nullptr;
    IDirect3DPixelShader9* m_rayBlur = nullptr;
    IDirect3DPixelShader9* m_probe = nullptr;
    IDirect3DVertexDeclaration9* m_decl = nullptr;
    IDirect3DStateBlock9* m_state = nullptr;

    IDirect3DTexture9* m_marchTarget = nullptr;
    IDirect3DTexture9* m_history[2] = {};
    IDirect3DTexture9* m_historyDepth = nullptr;
    IDirect3DTexture9* m_rays[2] = {};
    IDirect3DTexture9* m_sceneCopy = nullptr;
    IDirect3DTexture9* m_localLightData = nullptr;
    IDirect3DVolumeTexture9* m_densityNoise = nullptr;
    IDirect3DTexture9* m_fogAtlas = nullptr;
    IDirect3DTexture9* m_fogIntervals = nullptr;
    UINT m_atlasWidth = 0;
    UINT m_atlasHeight = 0;
    MaterialFogVolume m_materialVolume;
    IDirect3DTexture9* m_probeTarget = nullptr;
    IDirect3DSurface9* m_probeReadback = nullptr;
    UINT m_lowW = 0;
    UINT m_lowH = 0;
    UINT m_rayW = 0;
    UINT m_rayH = 0;
    UINT m_sceneCopyW = 0;
    UINT m_sceneCopyH = 0;
    bool m_sceneCopyFailed = false;
    bool m_probeFailed = false;

    int m_historyIndex = 0;
    bool m_historyValid = false;
    bool m_adaptiveLightingHistory = false;
    uint32_t m_prevLocalLightCount = 0;
    Config m_prevConfig;
    int m_prevMap = -1;
    int m_prevLightSlot = -1;
    int m_prevShadowMode = -1;
    float m_prevWorldToView[16] = {};
    float m_prevProj[16] = {};
    float m_prevCam[3] = {};
    D3DVIEWPORT9 m_prevViewport = {};
    UINT m_prevScale = 0;
    long long m_prevTicks = 0;
    unsigned m_frame = 0;
    unsigned m_logged = 0;
    long long m_summaryTicks = 0;
    long long m_probeTicks = 0;
    unsigned m_probeAttempts = 0;
    bool m_lightsLogged = false;
    float m_loggedFarClip = 0.0f;
    float m_loggedBlendMode = -1.0f;
    D3DVIEWPORT9 m_loggedViewport = {};
    uint32_t m_lightSignature = 0;
    const char* m_skip = "";
};
