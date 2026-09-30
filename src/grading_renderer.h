#pragma once

#include "fog_data.h"

#include <d3d9.h>

struct GradingStats
{
    unsigned grades = 0;
    unsigned curveUploads = 0;
    bool holdsSceneCopy = false;
    bool holdsCurve = false;
};

class GradingRenderer
{
public:
    ~GradingRenderer();

    void ReleaseDefaultPool();
    void ReleaseAll();

    bool Grade(IDirect3DDevice9* dev, const D3DVIEWPORT9& world, const float* curve, float strength);

    const char* LastSkipReason() const { return m_skip; }
    GradingStats Stats() const;

private:
    static constexpr DWORD kRenderTargets = 4;

    struct BoundTargets
    {
        IDirect3DSurface9* colour[kRenderTargets] = {};
        IDirect3DSurface9* depth = nullptr;
    };

    bool Skip(const char* reason);
    bool EnsureShaders(IDirect3DDevice9* dev);
    bool EnsureStateBlock(IDirect3DDevice9* dev);
    bool EnsureCurve(IDirect3DDevice9* dev, const float* curve);
    bool UploadCurve(const float* curve);
    bool EnsureSceneCopy(IDirect3DDevice9* dev, D3DFORMAT format, UINT width, UINT height);
    bool CopyWorld(IDirect3DDevice9* dev, IDirect3DSurface9* target, const D3DSURFACE_DESC& desc,
                   const D3DVIEWPORT9& world);
    bool GradeTarget(IDirect3DDevice9* dev, const BoundTargets& bound, const D3DVIEWPORT9& world, float strength);
    void DrawGraded(IDirect3DDevice9* dev, const BoundTargets& bound, const D3DVIEWPORT9& world, float strength);
    void SetGradingState(IDirect3DDevice9* dev, const D3DVIEWPORT9& world, float strength);

    IDirect3DVertexShader9* m_vs = nullptr;
    IDirect3DPixelShader9* m_ps = nullptr;
    IDirect3DVertexDeclaration9* m_decl = nullptr;
    IDirect3DStateBlock9* m_state = nullptr;
    IDirect3DTexture9* m_curve = nullptr;
    D3DFORMAT m_curveFormat = D3DFMT_UNKNOWN;
    float m_uploadedCurve[kGradingCurveEntries] = {};
    bool m_curveUploaded = false;
    IDirect3DTexture9* m_sceneCopy = nullptr;
    UINT m_sceneCopyWidth = 0;
    UINT m_sceneCopyHeight = 0;
    D3DFORMAT m_sceneCopyFormat = D3DFMT_UNKNOWN;
    bool m_sceneCopyFailed = false;
    IDirect3DDevice9* m_unsupportedDevice = nullptr;
    unsigned m_grades = 0;
    unsigned m_curveUploads = 0;
    const char* m_skip = "";
};
