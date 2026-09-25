#include "d3d9_wrap.h"

#include "log.h"
#include "overlay.h"
#include "renderer.h"

namespace
{
constexpr D3DFORMAT kIntz = static_cast<D3DFORMAT>(MAKEFOURCC('I', 'N', 'T', 'Z'));

constexpr int kMaxDevices = 8;

Direct3DCreate9Fn g_realCreate = nullptr;
bool g_fogAllowedOnNewDevices = false;
FogDevice* g_latestFogDevice = nullptr;
FogDevice* g_devices[kMaxDevices] = {};

void Register(FogDevice* device)
{
    for (auto*& slot : g_devices)
        if (!slot)
        {
            slot = device;
            return;
        }
}

void Unregister(FogDevice* device)
{
    for (auto*& slot : g_devices)
        if (slot == device)
            slot = nullptr;
    if (g_latestFogDevice == device)
        g_latestFogDevice = nullptr;
}

FogDevice* RegisteredWrapperOf(void* gameDevice)
{
    for (FogDevice* device : g_devices)
        if (IsWrapperOf(device, gameDevice))
            return device;
    return nullptr;
}

class WrappedD3D9;
}

class FogDevice final : public IDirect3DDevice9
{
public:
    FogDevice(WrappedD3D9* parent, IDirect3DDevice9* real, bool fog, D3DFORMAT depthFormat);

    bool FogActive() const { return m_fog && m_depthTexture; }
    IDirect3DDevice9* Real() const { return m_real; }
    bool CreateDepth();
    bool Render(const FrameInputs& in, const Config& cfg, const char** skip);
    bool AdaptiveLightingHistory() const { return m_renderer.AdaptiveLightingHistory(); }
    void ForceDepthWrite(bool force)
    {
        if (force == m_forceDepthWrite)
            return;
        if (force && FAILED(m_real->GetRenderState(D3DRS_ZWRITEENABLE, &m_clientRequestedDepthWrite)))
            return;
        m_forceDepthWrite = force;
        m_real->SetRenderState(D3DRS_ZWRITEENABLE, DepthWriteToApply());
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    HRESULT STDMETHODCALLTYPE TestCooperativeLevel() override { return m_real->TestCooperativeLevel(); }
    UINT STDMETHODCALLTYPE GetAvailableTextureMem() override { return m_real->GetAvailableTextureMem(); }
    HRESULT STDMETHODCALLTYPE EvictManagedResources() override { return m_real->EvictManagedResources(); }
    HRESULT STDMETHODCALLTYPE GetDirect3D(IDirect3D9** out) override;
    HRESULT STDMETHODCALLTYPE GetDeviceCaps(D3DCAPS9* caps) override { return m_real->GetDeviceCaps(caps); }
    HRESULT STDMETHODCALLTYPE GetDisplayMode(UINT sc, D3DDISPLAYMODE* mode) override
    {
        return m_real->GetDisplayMode(sc, mode);
    }
    HRESULT STDMETHODCALLTYPE GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS* p) override
    {
        return m_real->GetCreationParameters(p);
    }
    HRESULT STDMETHODCALLTYPE SetCursorProperties(UINT x, UINT y, IDirect3DSurface9* s) override
    {
        return m_real->SetCursorProperties(x, y, s);
    }
    void STDMETHODCALLTYPE SetCursorPosition(int x, int y, DWORD flags) override
    {
        m_real->SetCursorPosition(x, y, flags);
    }
    BOOL STDMETHODCALLTYPE ShowCursor(BOOL show) override { return m_real->ShowCursor(show); }
    HRESULT STDMETHODCALLTYPE CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* pp, IDirect3DSwapChain9** out) override
    {
        return m_real->CreateAdditionalSwapChain(pp, out);
    }
    HRESULT STDMETHODCALLTYPE GetSwapChain(UINT i, IDirect3DSwapChain9** out) override
    {
        return m_real->GetSwapChain(i, out);
    }
    UINT STDMETHODCALLTYPE GetNumberOfSwapChains() override { return m_real->GetNumberOfSwapChains(); }
    HRESULT STDMETHODCALLTYPE Reset(D3DPRESENT_PARAMETERS* pp) override;
    HRESULT STDMETHODCALLTYPE Present(const RECT* src, const RECT* dst, HWND wnd, const RGNDATA* dirty) override
    {
        DrawOverlay(m_real);
        return m_real->Present(src, dst, wnd, dirty);
    }
    HRESULT STDMETHODCALLTYPE GetBackBuffer(UINT sc, UINT i, D3DBACKBUFFER_TYPE t, IDirect3DSurface9** out) override
    {
        return m_real->GetBackBuffer(sc, i, t, out);
    }
    HRESULT STDMETHODCALLTYPE GetRasterStatus(UINT sc, D3DRASTER_STATUS* s) override
    {
        return m_real->GetRasterStatus(sc, s);
    }
    HRESULT STDMETHODCALLTYPE SetDialogBoxMode(BOOL e) override { return m_real->SetDialogBoxMode(e); }
    void STDMETHODCALLTYPE SetGammaRamp(UINT sc, DWORD flags, const D3DGAMMARAMP* r) override
    {
        m_real->SetGammaRamp(sc, flags, r);
    }
    void STDMETHODCALLTYPE GetGammaRamp(UINT sc, D3DGAMMARAMP* r) override { m_real->GetGammaRamp(sc, r); }
    HRESULT STDMETHODCALLTYPE CreateTexture(UINT w, UINT h, UINT levels, DWORD usage, D3DFORMAT fmt, D3DPOOL pool,
                                            IDirect3DTexture9** out, HANDLE* shared) override
    {
        return m_real->CreateTexture(w, h, levels, usage, fmt, pool, out, shared);
    }
    HRESULT STDMETHODCALLTYPE CreateVolumeTexture(UINT w, UINT h, UINT d, UINT levels, DWORD usage, D3DFORMAT fmt,
                                                  D3DPOOL pool, IDirect3DVolumeTexture9** out, HANDLE* shared) override
    {
        return m_real->CreateVolumeTexture(w, h, d, levels, usage, fmt, pool, out, shared);
    }
    HRESULT STDMETHODCALLTYPE CreateCubeTexture(UINT edge, UINT levels, DWORD usage, D3DFORMAT fmt, D3DPOOL pool,
                                                IDirect3DCubeTexture9** out, HANDLE* shared) override
    {
        return m_real->CreateCubeTexture(edge, levels, usage, fmt, pool, out, shared);
    }
    HRESULT STDMETHODCALLTYPE CreateVertexBuffer(UINT len, DWORD usage, DWORD fvf, D3DPOOL pool,
                                                 IDirect3DVertexBuffer9** out, HANDLE* shared) override
    {
        return m_real->CreateVertexBuffer(len, usage, fvf, pool, out, shared);
    }
    HRESULT STDMETHODCALLTYPE CreateIndexBuffer(UINT len, DWORD usage, D3DFORMAT fmt, D3DPOOL pool,
                                                IDirect3DIndexBuffer9** out, HANDLE* shared) override
    {
        return m_real->CreateIndexBuffer(len, usage, fmt, pool, out, shared);
    }
    HRESULT STDMETHODCALLTYPE CreateRenderTarget(UINT w, UINT h, D3DFORMAT fmt, D3DMULTISAMPLE_TYPE ms, DWORD q,
                                                 BOOL lockable, IDirect3DSurface9** out, HANDLE* shared) override
    {
        return m_real->CreateRenderTarget(w, h, fmt, ms, q, lockable, out, shared);
    }
    HRESULT STDMETHODCALLTYPE CreateDepthStencilSurface(UINT w, UINT h, D3DFORMAT fmt, D3DMULTISAMPLE_TYPE ms,
                                                        DWORD q, BOOL discard, IDirect3DSurface9** out,
                                                        HANDLE* shared) override
    {
        return m_real->CreateDepthStencilSurface(w, h, fmt, ms, q, discard, out, shared);
    }
    HRESULT STDMETHODCALLTYPE UpdateSurface(IDirect3DSurface9* src, const RECT* r, IDirect3DSurface9* dst,
                                            const POINT* p) override
    {
        return m_real->UpdateSurface(src, r, dst, p);
    }
    HRESULT STDMETHODCALLTYPE UpdateTexture(IDirect3DBaseTexture9* src, IDirect3DBaseTexture9* dst) override
    {
        return m_real->UpdateTexture(src, dst);
    }
    HRESULT STDMETHODCALLTYPE GetRenderTargetData(IDirect3DSurface9* rt, IDirect3DSurface9* dst) override
    {
        return m_real->GetRenderTargetData(rt, dst);
    }
    HRESULT STDMETHODCALLTYPE GetFrontBufferData(UINT sc, IDirect3DSurface9* dst) override
    {
        return m_real->GetFrontBufferData(sc, dst);
    }
    HRESULT STDMETHODCALLTYPE StretchRect(IDirect3DSurface9* src, const RECT* sr, IDirect3DSurface9* dst,
                                          const RECT* dr, D3DTEXTUREFILTERTYPE f) override
    {
        return m_real->StretchRect(src, sr, dst, dr, f);
    }
    HRESULT STDMETHODCALLTYPE ColorFill(IDirect3DSurface9* s, const RECT* r, D3DCOLOR c) override
    {
        return m_real->ColorFill(s, r, c);
    }
    HRESULT STDMETHODCALLTYPE CreateOffscreenPlainSurface(UINT w, UINT h, D3DFORMAT fmt, D3DPOOL pool,
                                                          IDirect3DSurface9** out, HANDLE* shared) override
    {
        return m_real->CreateOffscreenPlainSurface(w, h, fmt, pool, out, shared);
    }
    HRESULT STDMETHODCALLTYPE SetRenderTarget(DWORD i, IDirect3DSurface9* s) override
    {
        return m_real->SetRenderTarget(i, s);
    }
    HRESULT STDMETHODCALLTYPE GetRenderTarget(DWORD i, IDirect3DSurface9** out) override
    {
        return m_real->GetRenderTarget(i, out);
    }
    HRESULT STDMETHODCALLTYPE SetDepthStencilSurface(IDirect3DSurface9* s) override
    {
        return m_real->SetDepthStencilSurface(s);
    }
    HRESULT STDMETHODCALLTYPE GetDepthStencilSurface(IDirect3DSurface9** out) override
    {
        return m_real->GetDepthStencilSurface(out);
    }
    HRESULT STDMETHODCALLTYPE BeginScene() override { return m_real->BeginScene(); }
    HRESULT STDMETHODCALLTYPE EndScene() override { return m_real->EndScene(); }
    HRESULT STDMETHODCALLTYPE Clear(DWORD n, const D3DRECT* r, DWORD flags, D3DCOLOR c, float z, DWORD s) override
    {
        return m_real->Clear(n, r, flags, c, z, s);
    }
    HRESULT STDMETHODCALLTYPE SetTransform(D3DTRANSFORMSTATETYPE t, const D3DMATRIX* m) override
    {
        return m_real->SetTransform(t, m);
    }
    HRESULT STDMETHODCALLTYPE GetTransform(D3DTRANSFORMSTATETYPE t, D3DMATRIX* m) override
    {
        return m_real->GetTransform(t, m);
    }
    HRESULT STDMETHODCALLTYPE MultiplyTransform(D3DTRANSFORMSTATETYPE t, const D3DMATRIX* m) override
    {
        return m_real->MultiplyTransform(t, m);
    }
    HRESULT STDMETHODCALLTYPE SetViewport(const D3DVIEWPORT9* v) override { return m_real->SetViewport(v); }
    HRESULT STDMETHODCALLTYPE GetViewport(D3DVIEWPORT9* v) override { return m_real->GetViewport(v); }
    HRESULT STDMETHODCALLTYPE SetMaterial(const D3DMATERIAL9* m) override { return m_real->SetMaterial(m); }
    HRESULT STDMETHODCALLTYPE GetMaterial(D3DMATERIAL9* m) override { return m_real->GetMaterial(m); }
    HRESULT STDMETHODCALLTYPE SetLight(DWORD i, const D3DLIGHT9* l) override { return m_real->SetLight(i, l); }
    HRESULT STDMETHODCALLTYPE GetLight(DWORD i, D3DLIGHT9* l) override { return m_real->GetLight(i, l); }
    HRESULT STDMETHODCALLTYPE LightEnable(DWORD i, BOOL e) override { return m_real->LightEnable(i, e); }
    HRESULT STDMETHODCALLTYPE GetLightEnable(DWORD i, BOOL* e) override { return m_real->GetLightEnable(i, e); }
    HRESULT STDMETHODCALLTYPE SetClipPlane(DWORD i, const float* p) override { return m_real->SetClipPlane(i, p); }
    HRESULT STDMETHODCALLTYPE GetClipPlane(DWORD i, float* p) override { return m_real->GetClipPlane(i, p); }
    HRESULT STDMETHODCALLTYPE SetRenderState(D3DRENDERSTATETYPE s, DWORD v) override
    {
        if (s != D3DRS_ZWRITEENABLE)
            return m_real->SetRenderState(s, v);
        m_clientRequestedDepthWrite = v;
        return m_real->SetRenderState(s, DepthWriteToApply());
    }
    HRESULT STDMETHODCALLTYPE GetRenderState(D3DRENDERSTATETYPE s, DWORD* v) override
    {
        return m_real->GetRenderState(s, v);
    }
    HRESULT STDMETHODCALLTYPE CreateStateBlock(D3DSTATEBLOCKTYPE t, IDirect3DStateBlock9** out) override
    {
        return m_real->CreateStateBlock(t, out);
    }
    HRESULT STDMETHODCALLTYPE BeginStateBlock() override { return m_real->BeginStateBlock(); }
    HRESULT STDMETHODCALLTYPE EndStateBlock(IDirect3DStateBlock9** out) override
    {
        return m_real->EndStateBlock(out);
    }
    HRESULT STDMETHODCALLTYPE SetClipStatus(const D3DCLIPSTATUS9* s) override { return m_real->SetClipStatus(s); }
    HRESULT STDMETHODCALLTYPE GetClipStatus(D3DCLIPSTATUS9* s) override { return m_real->GetClipStatus(s); }
    HRESULT STDMETHODCALLTYPE GetTexture(DWORD i, IDirect3DBaseTexture9** out) override
    {
        return m_real->GetTexture(i, out);
    }
    HRESULT STDMETHODCALLTYPE SetTexture(DWORD i, IDirect3DBaseTexture9* t) override
    {
        return m_real->SetTexture(i, t);
    }
    HRESULT STDMETHODCALLTYPE GetTextureStageState(DWORD i, D3DTEXTURESTAGESTATETYPE t, DWORD* v) override
    {
        return m_real->GetTextureStageState(i, t, v);
    }
    HRESULT STDMETHODCALLTYPE SetTextureStageState(DWORD i, D3DTEXTURESTAGESTATETYPE t, DWORD v) override
    {
        return m_real->SetTextureStageState(i, t, v);
    }
    HRESULT STDMETHODCALLTYPE GetSamplerState(DWORD i, D3DSAMPLERSTATETYPE t, DWORD* v) override
    {
        return m_real->GetSamplerState(i, t, v);
    }
    HRESULT STDMETHODCALLTYPE SetSamplerState(DWORD i, D3DSAMPLERSTATETYPE t, DWORD v) override
    {
        return m_real->SetSamplerState(i, t, v);
    }
    HRESULT STDMETHODCALLTYPE ValidateDevice(DWORD* passes) override { return m_real->ValidateDevice(passes); }
    HRESULT STDMETHODCALLTYPE SetPaletteEntries(UINT n, const PALETTEENTRY* e) override
    {
        return m_real->SetPaletteEntries(n, e);
    }
    HRESULT STDMETHODCALLTYPE GetPaletteEntries(UINT n, PALETTEENTRY* e) override
    {
        return m_real->GetPaletteEntries(n, e);
    }
    HRESULT STDMETHODCALLTYPE SetCurrentTexturePalette(UINT n) override
    {
        return m_real->SetCurrentTexturePalette(n);
    }
    HRESULT STDMETHODCALLTYPE GetCurrentTexturePalette(UINT* n) override
    {
        return m_real->GetCurrentTexturePalette(n);
    }
    HRESULT STDMETHODCALLTYPE SetScissorRect(const RECT* r) override { return m_real->SetScissorRect(r); }
    HRESULT STDMETHODCALLTYPE GetScissorRect(RECT* r) override { return m_real->GetScissorRect(r); }
    HRESULT STDMETHODCALLTYPE SetSoftwareVertexProcessing(BOOL s) override
    {
        return m_real->SetSoftwareVertexProcessing(s);
    }
    BOOL STDMETHODCALLTYPE GetSoftwareVertexProcessing() override { return m_real->GetSoftwareVertexProcessing(); }
    HRESULT STDMETHODCALLTYPE SetNPatchMode(float n) override { return m_real->SetNPatchMode(n); }
    float STDMETHODCALLTYPE GetNPatchMode() override { return m_real->GetNPatchMode(); }
    HRESULT STDMETHODCALLTYPE DrawPrimitive(D3DPRIMITIVETYPE t, UINT start, UINT count) override
    {
        return m_real->DrawPrimitive(t, start, count);
    }
    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitive(D3DPRIMITIVETYPE t, INT base, UINT minIndex, UINT vertices,
                                                   UINT start, UINT count) override
    {
        return m_real->DrawIndexedPrimitive(t, base, minIndex, vertices, start, count);
    }
    HRESULT STDMETHODCALLTYPE DrawPrimitiveUP(D3DPRIMITIVETYPE t, UINT count, const void* data, UINT stride) override
    {
        return m_real->DrawPrimitiveUP(t, count, data, stride);
    }
    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE t, UINT minIndex, UINT vertices, UINT count,
                                                     const void* indices, D3DFORMAT fmt, const void* data,
                                                     UINT stride) override
    {
        return m_real->DrawIndexedPrimitiveUP(t, minIndex, vertices, count, indices, fmt, data, stride);
    }
    HRESULT STDMETHODCALLTYPE ProcessVertices(UINT src, UINT dst, UINT count, IDirect3DVertexBuffer9* buffer,
                                              IDirect3DVertexDeclaration9* decl, DWORD flags) override
    {
        return m_real->ProcessVertices(src, dst, count, buffer, decl, flags);
    }
    HRESULT STDMETHODCALLTYPE CreateVertexDeclaration(const D3DVERTEXELEMENT9* e,
                                                      IDirect3DVertexDeclaration9** out) override
    {
        return m_real->CreateVertexDeclaration(e, out);
    }
    HRESULT STDMETHODCALLTYPE SetVertexDeclaration(IDirect3DVertexDeclaration9* d) override
    {
        return m_real->SetVertexDeclaration(d);
    }
    HRESULT STDMETHODCALLTYPE GetVertexDeclaration(IDirect3DVertexDeclaration9** out) override
    {
        return m_real->GetVertexDeclaration(out);
    }
    HRESULT STDMETHODCALLTYPE SetFVF(DWORD fvf) override { return m_real->SetFVF(fvf); }
    HRESULT STDMETHODCALLTYPE GetFVF(DWORD* fvf) override { return m_real->GetFVF(fvf); }
    HRESULT STDMETHODCALLTYPE CreateVertexShader(const DWORD* code, IDirect3DVertexShader9** out) override
    {
        return m_real->CreateVertexShader(code, out);
    }
    HRESULT STDMETHODCALLTYPE SetVertexShader(IDirect3DVertexShader9* s) override
    {
        return m_real->SetVertexShader(s);
    }
    HRESULT STDMETHODCALLTYPE GetVertexShader(IDirect3DVertexShader9** out) override
    {
        return m_real->GetVertexShader(out);
    }
    HRESULT STDMETHODCALLTYPE SetVertexShaderConstantF(UINT r, const float* d, UINT n) override
    {
        return m_real->SetVertexShaderConstantF(r, d, n);
    }
    HRESULT STDMETHODCALLTYPE GetVertexShaderConstantF(UINT r, float* d, UINT n) override
    {
        return m_real->GetVertexShaderConstantF(r, d, n);
    }
    HRESULT STDMETHODCALLTYPE SetVertexShaderConstantI(UINT r, const int* d, UINT n) override
    {
        return m_real->SetVertexShaderConstantI(r, d, n);
    }
    HRESULT STDMETHODCALLTYPE GetVertexShaderConstantI(UINT r, int* d, UINT n) override
    {
        return m_real->GetVertexShaderConstantI(r, d, n);
    }
    HRESULT STDMETHODCALLTYPE SetVertexShaderConstantB(UINT r, const BOOL* d, UINT n) override
    {
        return m_real->SetVertexShaderConstantB(r, d, n);
    }
    HRESULT STDMETHODCALLTYPE GetVertexShaderConstantB(UINT r, BOOL* d, UINT n) override
    {
        return m_real->GetVertexShaderConstantB(r, d, n);
    }
    HRESULT STDMETHODCALLTYPE SetStreamSource(UINT i, IDirect3DVertexBuffer9* b, UINT offset, UINT stride) override
    {
        return m_real->SetStreamSource(i, b, offset, stride);
    }
    HRESULT STDMETHODCALLTYPE GetStreamSource(UINT i, IDirect3DVertexBuffer9** b, UINT* offset,
                                              UINT* stride) override
    {
        return m_real->GetStreamSource(i, b, offset, stride);
    }
    HRESULT STDMETHODCALLTYPE SetStreamSourceFreq(UINT i, UINT d) override
    {
        return m_real->SetStreamSourceFreq(i, d);
    }
    HRESULT STDMETHODCALLTYPE GetStreamSourceFreq(UINT i, UINT* d) override
    {
        return m_real->GetStreamSourceFreq(i, d);
    }
    HRESULT STDMETHODCALLTYPE SetIndices(IDirect3DIndexBuffer9* b) override { return m_real->SetIndices(b); }
    HRESULT STDMETHODCALLTYPE GetIndices(IDirect3DIndexBuffer9** b) override { return m_real->GetIndices(b); }
    HRESULT STDMETHODCALLTYPE CreatePixelShader(const DWORD* code, IDirect3DPixelShader9** out) override
    {
        return m_real->CreatePixelShader(code, out);
    }
    HRESULT STDMETHODCALLTYPE SetPixelShader(IDirect3DPixelShader9* s) override { return m_real->SetPixelShader(s); }
    HRESULT STDMETHODCALLTYPE GetPixelShader(IDirect3DPixelShader9** out) override
    {
        return m_real->GetPixelShader(out);
    }
    HRESULT STDMETHODCALLTYPE SetPixelShaderConstantF(UINT r, const float* d, UINT n) override
    {
        return m_real->SetPixelShaderConstantF(r, d, n);
    }
    HRESULT STDMETHODCALLTYPE GetPixelShaderConstantF(UINT r, float* d, UINT n) override
    {
        return m_real->GetPixelShaderConstantF(r, d, n);
    }
    HRESULT STDMETHODCALLTYPE SetPixelShaderConstantI(UINT r, const int* d, UINT n) override
    {
        return m_real->SetPixelShaderConstantI(r, d, n);
    }
    HRESULT STDMETHODCALLTYPE GetPixelShaderConstantI(UINT r, int* d, UINT n) override
    {
        return m_real->GetPixelShaderConstantI(r, d, n);
    }
    HRESULT STDMETHODCALLTYPE SetPixelShaderConstantB(UINT r, const BOOL* d, UINT n) override
    {
        return m_real->SetPixelShaderConstantB(r, d, n);
    }
    HRESULT STDMETHODCALLTYPE GetPixelShaderConstantB(UINT r, BOOL* d, UINT n) override
    {
        return m_real->GetPixelShaderConstantB(r, d, n);
    }
    HRESULT STDMETHODCALLTYPE DrawRectPatch(UINT h, const float* s, const D3DRECTPATCH_INFO* i) override
    {
        return m_real->DrawRectPatch(h, s, i);
    }
    HRESULT STDMETHODCALLTYPE DrawTriPatch(UINT h, const float* s, const D3DTRIPATCH_INFO* i) override
    {
        return m_real->DrawTriPatch(h, s, i);
    }
    HRESULT STDMETHODCALLTYPE DeletePatch(UINT h) override { return m_real->DeletePatch(h); }
    HRESULT STDMETHODCALLTYPE CreateQuery(D3DQUERYTYPE t, IDirect3DQuery9** out) override
    {
        return m_real->CreateQuery(t, out);
    }

private:
    ~FogDevice();
    void ReleaseDepth();
    bool BindFallbackDepth();
    DWORD DepthWriteToApply() const { return m_forceDepthWrite ? TRUE : m_clientRequestedDepthWrite; }

    LONG m_ref = 1;
    DWORD m_clientRequestedDepthWrite = TRUE;
    bool m_forceDepthWrite = false;
    WrappedD3D9* m_parent;
    IDirect3DDevice9* m_real;
    bool m_fog;
    D3DFORMAT m_depthFormat;
    IDirect3DTexture9* m_depthTexture = nullptr;
    IDirect3DSurface9* m_depthSurface = nullptr;
    Renderer m_renderer;
};

namespace
{
void ApplyFogParameters(D3DPRESENT_PARAMETERS& pp)
{
    pp.EnableAutoDepthStencil = FALSE;
    pp.MultiSampleType = D3DMULTISAMPLE_NONE;
    pp.MultiSampleQuality = 0;
}

HWND DeviceWindow(HWND focusWindow, const D3DPRESENT_PARAMETERS& pp)
{
    return pp.hDeviceWindow ? pp.hDeviceWindow : focusWindow;
}

void CopyBackParameters(D3DPRESENT_PARAMETERS* engine, const D3DPRESENT_PARAMETERS& used)
{
    D3DPRESENT_PARAMETERS copy = used;
    copy.EnableAutoDepthStencil = engine->EnableAutoDepthStencil;
    copy.AutoDepthStencilFormat = engine->AutoDepthStencilFormat;
    *engine = copy;
}

class WrappedD3D9 final : public IDirect3D9
{
public:
    explicit WrappedD3D9(IDirect3D9* real) : m_real(real) {}

    IDirect3D9* Real() const { return m_real; }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override
    {
        if (!out)
            return E_POINTER;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IDirect3D9))
        {
            AddRef();
            *out = this;
            return S_OK;
        }
        return m_real->QueryInterface(riid, out);
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&m_ref)); }
    ULONG STDMETHODCALLTYPE Release() override
    {
        LONG r = InterlockedDecrement(&m_ref);
        if (r == 0)
        {
            m_real->Release();
            delete this;
        }
        return static_cast<ULONG>(r);
    }
    HRESULT STDMETHODCALLTYPE RegisterSoftwareDevice(void* init) override
    {
        return m_real->RegisterSoftwareDevice(init);
    }
    UINT STDMETHODCALLTYPE GetAdapterCount() override { return m_real->GetAdapterCount(); }
    HRESULT STDMETHODCALLTYPE GetAdapterIdentifier(UINT a, DWORD f, D3DADAPTER_IDENTIFIER9* id) override
    {
        return m_real->GetAdapterIdentifier(a, f, id);
    }
    UINT STDMETHODCALLTYPE GetAdapterModeCount(UINT a, D3DFORMAT f) override
    {
        return m_real->GetAdapterModeCount(a, f);
    }
    HRESULT STDMETHODCALLTYPE EnumAdapterModes(UINT a, D3DFORMAT f, UINT m, D3DDISPLAYMODE* mode) override
    {
        return m_real->EnumAdapterModes(a, f, m, mode);
    }
    HRESULT STDMETHODCALLTYPE GetAdapterDisplayMode(UINT a, D3DDISPLAYMODE* mode) override
    {
        return m_real->GetAdapterDisplayMode(a, mode);
    }
    HRESULT STDMETHODCALLTYPE CheckDeviceType(UINT a, D3DDEVTYPE t, D3DFORMAT af, D3DFORMAT bf, BOOL w) override
    {
        return m_real->CheckDeviceType(a, t, af, bf, w);
    }
    HRESULT STDMETHODCALLTYPE CheckDeviceFormat(UINT a, D3DDEVTYPE t, D3DFORMAT af, DWORD u, D3DRESOURCETYPE r,
                                                D3DFORMAT f) override
    {
        return m_real->CheckDeviceFormat(a, t, af, u, r, f);
    }
    HRESULT STDMETHODCALLTYPE CheckDeviceMultiSampleType(UINT a, D3DDEVTYPE t, D3DFORMAT f, BOOL w,
                                                         D3DMULTISAMPLE_TYPE ms, DWORD* q) override
    {
        if (g_fogAllowedOnNewDevices && ms != D3DMULTISAMPLE_NONE)
            return D3DERR_NOTAVAILABLE;
        return m_real->CheckDeviceMultiSampleType(a, t, f, w, ms, q);
    }
    HRESULT STDMETHODCALLTYPE CheckDepthStencilMatch(UINT a, D3DDEVTYPE t, D3DFORMAT af, D3DFORMAT rf,
                                                     D3DFORMAT df) override
    {
        return m_real->CheckDepthStencilMatch(a, t, af, rf, df);
    }
    HRESULT STDMETHODCALLTYPE CheckDeviceFormatConversion(UINT a, D3DDEVTYPE t, D3DFORMAT s, D3DFORMAT d) override
    {
        return m_real->CheckDeviceFormatConversion(a, t, s, d);
    }
    HRESULT STDMETHODCALLTYPE GetDeviceCaps(UINT a, D3DDEVTYPE t, D3DCAPS9* caps) override
    {
        return m_real->GetDeviceCaps(a, t, caps);
    }
    HMONITOR STDMETHODCALLTYPE GetAdapterMonitor(UINT a) override { return m_real->GetAdapterMonitor(a); }
    HRESULT STDMETHODCALLTYPE CreateDevice(UINT adapter, D3DDEVTYPE type, HWND window, DWORD flags,
                                           D3DPRESENT_PARAMETERS* pp, IDirect3DDevice9** out) override;

private:
    bool SupportsIntz(UINT adapter, D3DDEVTYPE type, const D3DPRESENT_PARAMETERS& pp)
    {
        D3DFORMAT adapterFormat = pp.BackBufferFormat;
        D3DDISPLAYMODE mode;
        if (pp.Windowed || adapterFormat == D3DFMT_UNKNOWN)
            if (SUCCEEDED(m_real->GetAdapterDisplayMode(adapter, &mode)))
                adapterFormat = mode.Format;
        return SUCCEEDED(m_real->CheckDeviceFormat(adapter, type, adapterFormat, D3DUSAGE_DEPTHSTENCIL,
                                                   D3DRTYPE_TEXTURE, kIntz));
    }

    ~WrappedD3D9() = default;

    LONG m_ref = 1;
    IDirect3D9* m_real;
};

HRESULT WrappedD3D9::CreateDevice(UINT adapter, D3DDEVTYPE type, HWND window, DWORD flags,
                                  D3DPRESENT_PARAMETERS* pp, IDirect3DDevice9** out)
{
    if (!pp || !out)
        return D3DERR_INVALIDCALL;
    bool fog = g_fogAllowedOnNewDevices && GlobalConfig().Get().enable && pp->EnableAutoDepthStencil;
    if (fog && !SupportsIntz(adapter, type, *pp))
    {
        VF_LOG_ERROR("INTZ depth textures are not supported on this adapter; fog disabled");
        fog = false;
    }

    D3DPRESENT_PARAMETERS used = *pp;
    DWORD usedFlags = flags;
    if (fog)
    {
        ApplyFogParameters(used);
        usedFlags &= ~static_cast<DWORD>(D3DCREATE_PUREDEVICE);
    }
    IDirect3DDevice9* real = nullptr;
    HRESULT hr = m_real->CreateDevice(adapter, type, window, usedFlags, &used, &real);
    if (FAILED(hr) && fog)
    {
        VF_LOG_ERROR("CreateDevice with fog parameters failed (0x%08lX); retrying unchanged", hr);
        fog = false;
        used = *pp;
        hr = m_real->CreateDevice(adapter, type, window, flags, &used, &real);
    }
    if (FAILED(hr))
        return hr;
    CopyBackParameters(pp, used);

    auto* device = new FogDevice(this, real, fog, pp->AutoDepthStencilFormat);
    if (fog && !device->CreateDepth())
        VF_LOG_ERROR("fog depth could not be created; fog disabled for this device");
    VF_LOG_INFO("CreateDevice: %lux%lu windowed=%d flags 0x%02lX -> 0x%02lX ms=%d fog=%d", used.BackBufferWidth,
                used.BackBufferHeight, used.Windowed, flags, usedFlags, pp->MultiSampleType,
                device->FogActive() ? 1 : 0);
    if (device->FogActive())
        g_latestFogDevice = device;
    if (device->FogActive() && GlobalConfig().Get().overlay)
        AttachOverlay(real, DeviceWindow(window, *pp));
    *out = device;
    return hr;
}
}

FogDevice::FogDevice(WrappedD3D9* parent, IDirect3DDevice9* real, bool fog, D3DFORMAT depthFormat)
    : m_parent(parent), m_real(real), m_fog(fog), m_depthFormat(depthFormat)
{
    m_parent->AddRef();
    Register(this);
}

FogDevice::~FogDevice()
{
    DetachOverlay(m_real);
    Unregister(this);
    m_renderer.ReleaseAll();
    ReleaseDepth();
    m_real->Release();
    m_parent->Release();
}

void FogDevice::ReleaseDepth()
{
    if (m_depthSurface)
    {
        m_depthSurface->Release();
        m_depthSurface = nullptr;
    }
    if (m_depthTexture)
    {
        m_depthTexture->Release();
        m_depthTexture = nullptr;
    }
}

bool FogDevice::BindFallbackDepth()
{
    IDirect3DSurface9* backBuffer = nullptr;
    if (FAILED(m_real->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)))
        return false;
    D3DSURFACE_DESC desc;
    backBuffer->GetDesc(&desc);
    backBuffer->Release();
    IDirect3DSurface9* depth = nullptr;
    D3DFORMAT format = m_depthFormat != D3DFMT_UNKNOWN ? m_depthFormat : D3DFMT_D24S8;
    if (FAILED(m_real->CreateDepthStencilSurface(desc.Width, desc.Height, format, D3DMULTISAMPLE_NONE, 0, FALSE,
                                                 &depth, nullptr)))
        return false;
    m_real->SetDepthStencilSurface(depth);
    depth->Release();
    return true;
}

bool FogDevice::CreateDepth()
{
    ReleaseDepth();
    IDirect3DSurface9* backBuffer = nullptr;
    D3DSURFACE_DESC desc = {};
    if (SUCCEEDED(m_real->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)))
    {
        backBuffer->GetDesc(&desc);
        backBuffer->Release();
    }
    if (desc.Width && SUCCEEDED(m_real->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_DEPTHSTENCIL, kIntz,
                                                      D3DPOOL_DEFAULT, &m_depthTexture, nullptr)) &&
        SUCCEEDED(m_depthTexture->GetSurfaceLevel(0, &m_depthSurface)) &&
        SUCCEEDED(m_real->SetDepthStencilSurface(m_depthSurface)))
        return true;

    ReleaseDepth();
    m_fog = false;
    if (g_latestFogDevice == this)
        g_latestFogDevice = nullptr;
    BindFallbackDepth();
    return false;
}

HRESULT FogDevice::QueryInterface(REFIID riid, void** out)
{
    if (!out)
        return E_POINTER;
    if (riid == __uuidof(IUnknown) || riid == __uuidof(IDirect3DDevice9))
    {
        AddRef();
        *out = this;
        return S_OK;
    }
    return m_real->QueryInterface(riid, out);
}

ULONG FogDevice::AddRef()
{
    return static_cast<ULONG>(InterlockedIncrement(&m_ref));
}

ULONG FogDevice::Release()
{
    LONG r = InterlockedDecrement(&m_ref);
    if (r == 0)
        delete this;
    return static_cast<ULONG>(r);
}

HRESULT FogDevice::GetDirect3D(IDirect3D9** out)
{
    if (!out)
        return D3DERR_INVALIDCALL;
    m_parent->AddRef();
    *out = m_parent;
    return D3D_OK;
}

HRESULT FogDevice::Reset(D3DPRESENT_PARAMETERS* pp)
{
    if (!pp)
        return D3DERR_INVALIDCALL;
    ReleaseOverlayDeviceObjects(m_real);
    if (!m_fog)
        return m_real->Reset(pp);

    m_renderer.ReleaseDefaultPool();
    ReleaseDepth();
    D3DPRESENT_PARAMETERS used = *pp;
    ApplyFogParameters(used);
    HRESULT hr = m_real->Reset(&used);
    if (FAILED(hr))
    {
        VF_LOG_ERROR("Reset failed (0x%08lX)", hr);
        return hr;
    }
    CopyBackParameters(pp, used);
    if (!CreateDepth())
        VF_LOG_ERROR("fog depth could not be recreated after Reset; fog disabled");
    VF_LOG_INFO("Reset: %lux%lu fog=%d", used.BackBufferWidth, used.BackBufferHeight, FogActive() ? 1 : 0);
    return hr;
}

bool FogDevice::Render(const FrameInputs& in, const Config& cfg, const char** skip)
{
    bool ok = FogActive() && m_renderer.Render(m_real, m_depthTexture, m_depthSurface, in, cfg);
    if (skip)
        *skip = FogActive() ? m_renderer.LastSkipReason() : "fog inactive";
    return ok;
}

void SetRealDirect3DCreate9(Direct3DCreate9Fn fn)
{
    g_realCreate = fn;
}

IDirect3D9* WINAPI WrappedDirect3DCreate9(UINT sdkVersion)
{
    if (!g_realCreate)
        return nullptr;
    IDirect3D9* real = g_realCreate(sdkVersion);
    if (!real)
        return nullptr;
    return new WrappedD3D9(real);
}

void AllowFogOnNewDevices(bool allowedOnNewDevices)
{
    g_fogAllowedOnNewDevices = allowedOnNewDevices;
}

FogDevice* LatestFogDevice()
{
    return g_latestFogDevice;
}

bool IsWrapperOf(FogDevice* device, void* gameDevice)
{
    return device && static_cast<IDirect3DDevice9*>(device) == gameDevice;
}

FogDevice* WrapperOrLatestFogDevice(void* gameDevice)
{
    FogDevice* wrapper = RegisteredWrapperOf(gameDevice);
    if (!wrapper)
        return g_latestFogDevice;
    return wrapper->FogActive() ? wrapper : nullptr;
}

IDirect3DDevice9* RealDevice(FogDevice* device)
{
    return device ? device->Real() : nullptr;
}

void ForceDepthWrite(FogDevice* device, bool force)
{
    if (device)
        device->ForceDepthWrite(force);
}

bool RenderFog(FogDevice* device, const FrameInputs& in, const Config& cfg, const char** skipReason)
{
    return device && device->Render(in, cfg, skipReason);
}

bool AdaptiveLightingHistory(FogDevice* device)
{
    return device && device->AdaptiveLightingHistory();
}
