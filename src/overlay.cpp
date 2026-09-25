#include "overlay.h"

#include "config.h"
#include "hooks.h"
#include "log.h"
#include "settings_panel.h"

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#include <windowsx.h>

#include <algorithm>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND window, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
constexpr LPARAM kKeyWasDownBit = static_cast<LPARAM>(1) << 30;
constexpr DWORD kMaxRenderTargets = 4;
constexpr DWORD kAllColorChannels =
    D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA;
constexpr float kPanelFontPixels = 16.0f;
constexpr float kReferenceBackBufferHeight = 1080.0f;
constexpr float kMinFrameSeconds = 1.0f / 1000.0f;

struct OverlayState
{
    IDirect3DDevice9* device = nullptr;
    bool visible = false;
    bool failed = false;
    unsigned swallowedKey = 0;
    UINT backBufferWidth = 0;
    UINT backBufferHeight = 0;
    float scale = 0.0f;
    SettingsPanel panel;
};

struct SubclassedWindow
{
    HWND window = nullptr;
    WNDPROC clientProc = nullptr;
    bool unicode = false;
};

OverlayState g_overlay;
SubclassedWindow g_subclass;

LRESULT CALLBACK OverlayWindowProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam);

LONG_PTR CurrentWindowProc(const SubclassedWindow& s)
{
    return s.unicode ? GetWindowLongPtrW(s.window, GWLP_WNDPROC) : GetWindowLongPtrA(s.window, GWLP_WNDPROC);
}

LONG_PTR ReplaceWindowProc(HWND window, bool unicode, LONG_PTR proc)
{
    return unicode ? SetWindowLongPtrW(window, GWLP_WNDPROC, proc) : SetWindowLongPtrA(window, GWLP_WNDPROC, proc);
}

bool RestoreClientWindowProc()
{
    if (!g_subclass.window)
        return true;
    if (CurrentWindowProc(g_subclass) != reinterpret_cast<LONG_PTR>(&OverlayWindowProc))
        return false;
    ReplaceWindowProc(g_subclass.window, g_subclass.unicode, reinterpret_cast<LONG_PTR>(g_subclass.clientProc));
    g_subclass = SubclassedWindow();
    return true;
}

bool ChainOverlayWindowProc(HWND window)
{
    if (g_subclass.window == window)
        return true;
    if (!RestoreClientWindowProc())
        return false;
    const bool unicode = IsWindowUnicode(window) != FALSE;
    const LONG_PTR clientProc = ReplaceWindowProc(window, unicode, reinterpret_cast<LONG_PTR>(&OverlayWindowProc));
    if (!clientProc)
        return false;
    g_subclass.window = window;
    g_subclass.clientProc = reinterpret_cast<WNDPROC>(clientProc);
    g_subclass.unicode = unicode;
    return true;
}

POINT ClientToBackBuffer(HWND window, POINT p)
{
    RECT client;
    if (!g_overlay.backBufferWidth || !GetClientRect(window, &client) || client.right <= 0 || client.bottom <= 0)
        return p;
    return {MulDiv(p.x, static_cast<int>(g_overlay.backBufferWidth), client.right),
            MulDiv(p.y, static_cast<int>(g_overlay.backBufferHeight), client.bottom)};
}

LPARAM InBackBufferPixels(HWND window, UINT msg, LPARAM lParam)
{
    if (msg != WM_MOUSEMOVE)
        return lParam;
    const POINT p = ClientToBackBuffer(window, {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
    return MAKELPARAM(p.x, p.y);
}

void FeedCursorPosition(HWND window)
{
    POINT p;
    if (!window || !GetCursorPos(&p) || !ScreenToClient(window, &p))
        return;
    p = ClientToBackBuffer(window, p);
    ImGui::GetIO().AddMousePosEvent(static_cast<float>(p.x), static_cast<float>(p.y));
}

void SetVisible(bool visible)
{
    if (visible == g_overlay.visible || !ImGui::GetCurrentContext())
        return;
    g_overlay.visible = visible;
    ImGuiIO& io = ImGui::GetIO();
    io.ClearInputKeys();
    io.ClearInputMouse();
    if (visible)
        FeedCursorPosition(g_subclass.window);
}

bool IsKeyDownMessage(UINT msg)
{
    return msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN;
}

bool IsKeyUpMessage(UINT msg)
{
    return msg == WM_KEYUP || msg == WM_SYSKEYUP;
}

bool IsCharacterMessage(UINT msg)
{
    return msg == WM_CHAR || msg == WM_SYSCHAR || msg == WM_DEADCHAR || msg == WM_SYSDEADCHAR;
}

bool IsMouseClickOrWheelMessage(UINT msg)
{
    return msg > WM_MOUSEMOVE && msg <= WM_MOUSELAST;
}

bool ModifierDown(int virtualKey)
{
    return GetKeyState(virtualKey) < 0;
}

bool HotkeyPressed(const Hotkey& key, UINT msg, WPARAM wParam)
{
    return IsKeyDownMessage(msg) && wParam == key.virtualKey && ModifierDown(VK_CONTROL) == key.ctrl &&
           ModifierDown(VK_SHIFT) == key.shift && ModifierDown(VK_MENU) == key.alt;
}

bool TakesHotkey(const Hotkey& key, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (HotkeyPressed(key, msg, wParam))
    {
        if (!(lParam & kKeyWasDownBit))
            SetVisible(!g_overlay.visible);
        g_overlay.swallowedKey = key.virtualKey;
        return true;
    }
    if (!g_overlay.swallowedKey)
        return false;
    if (IsKeyUpMessage(msg) && wParam == g_overlay.swallowedKey)
    {
        g_overlay.swallowedKey = 0;
        return true;
    }
    return IsCharacterMessage(msg);
}

bool PanelWantsMessage(UINT msg)
{
    const ImGuiIO& io = ImGui::GetIO();
    if (IsMouseClickOrWheelMessage(msg))
        return io.WantCaptureMouse;
    if (IsKeyDownMessage(msg) || IsCharacterMessage(msg))
        return io.WantCaptureKeyboard;
    return false;
}

bool OverlayTakesMessage(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (g_overlay.failed || !g_overlay.device)
        return false;
    const Config& cfg = GlobalConfig().Get();
    if (!cfg.overlay)
    {
        SetVisible(false);
        return false;
    }
    if (TakesHotkey(cfg.overlayKey, msg, wParam, lParam))
        return true;
    if (!g_overlay.visible)
        return false;
    ImGui_ImplWin32_WndProcHandler(window, msg, wParam, InBackBufferPixels(window, msg, lParam));
    return PanelWantsMessage(msg);
}

int OverlayFaultFilter(unsigned code, const char* where)
{
    VF_LOG_ERROR("exception 0x%08X in the overlay %s; overlay disabled for this session", code, where);
    return EXCEPTION_EXECUTE_HANDLER;
}

bool OverlayTakesMessageGuarded(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    __try
    {
        return OverlayTakesMessage(window, msg, wParam, lParam);
    }
    __except (OverlayFaultFilter(GetExceptionCode(), "window message"))
    {
        g_overlay.failed = true;
        g_overlay.visible = false;
        return false;
    }
}

LRESULT CALLBACK OverlayWindowProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    const SubclassedWindow subclass = g_subclass;
    if (window == subclass.window && OverlayTakesMessageGuarded(window, msg, wParam, lParam))
        return 0;
    if (msg == WM_NCDESTROY && window == subclass.window)
        g_subclass = SubclassedWindow();
    return subclass.unicode ? CallWindowProcW(subclass.clientProc, window, msg, wParam, lParam)
                            : CallWindowProcA(subclass.clientProc, window, msg, wParam, lParam);
}

void ConfigureImGui()
{
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    ImFontConfig font;
    font.SizePixels = kPanelFontPixels;
    io.Fonts->AddFontDefaultVector(&font);
    g_overlay.scale = 0.0f;
}

void ApplyScale(float scale)
{
    if (scale == g_overlay.scale)
        return;
    g_overlay.scale = scale;
    ImGuiStyle style;
    ImGui::StyleColorsDark(&style);
    style.ScaleAllSizes(scale);
    style.FontScaleDpi = scale;
    ImGui::GetStyle() = style;
}

void BuildPanelFrame(const D3DSURFACE_DESC& backBuffer)
{
    ApplyScale(std::max(1.0f, static_cast<float>(backBuffer.Height) / kReferenceBackBufferHeight));
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(backBuffer.Width), static_cast<float>(backBuffer.Height));
    io.DeltaTime = std::max(io.DeltaTime, kMinFrameSeconds);
    ImGui::NewFrame();
    bool open = true;
    ConfigStore& store = GlobalConfig();
    g_overlay.panel.Draw(store, LastFogFrameStatus(), HotkeyName(store.Get().overlayKey), open);
    ImGui::Render();
    if (!open)
        SetVisible(false);
}

class SavedDeviceState
{
public:
    ~SavedDeviceState()
    {
        for (IDirect3DSurface9* target : m_targets)
            if (target)
                target->Release();
        if (m_depth)
            m_depth->Release();
        if (m_stream)
            m_stream->Release();
        if (m_indices)
            m_indices->Release();
        if (m_state)
            m_state->Release();
    }

    bool Capture(IDirect3DDevice9* device)
    {
        if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &m_state)) || FAILED(m_state->Capture()))
            return false;
        for (DWORD i = 0; i < kMaxRenderTargets; ++i)
            device->GetRenderTarget(i, &m_targets[i]);
        device->GetDepthStencilSurface(&m_depth);
        device->GetStreamSource(0, &m_stream, &m_streamOffset, &m_streamStride);
        device->GetIndices(&m_indices);
        device->GetViewport(&m_viewport);
        device->GetScissorRect(&m_scissor);
        return m_targets[0] != nullptr;
    }

    IDirect3DSurface9* ExtraTarget(DWORD index) const { return m_targets[index]; }

    void Restore(IDirect3DDevice9* device)
    {
        for (DWORD i = 0; i < kMaxRenderTargets; ++i)
            if (m_targets[i])
                device->SetRenderTarget(i, m_targets[i]);
        device->SetDepthStencilSurface(m_depth);
        m_state->Apply();
        device->SetStreamSource(0, m_stream, m_streamOffset, m_streamStride);
        device->SetIndices(m_indices);
        device->SetViewport(&m_viewport);
        device->SetScissorRect(&m_scissor);
    }

private:
    IDirect3DStateBlock9* m_state = nullptr;
    IDirect3DSurface9* m_targets[kMaxRenderTargets] = {};
    IDirect3DSurface9* m_depth = nullptr;
    IDirect3DVertexBuffer9* m_stream = nullptr;
    UINT m_streamOffset = 0;
    UINT m_streamStride = 0;
    IDirect3DIndexBuffer9* m_indices = nullptr;
    D3DVIEWPORT9 m_viewport = {};
    RECT m_scissor = {};
};

void SetStateImGuiLeavesAlone(IDirect3DDevice9* device)
{
    device->SetRenderState(D3DRS_COLORWRITEENABLE, kAllColorChannels);
    device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
    device->SetRenderState(D3DRS_WRAP0, 0);
    device->SetRenderState(D3DRS_VERTEXBLEND, D3DVBF_DISABLE);
    device->SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_BLENDOPALPHA, D3DBLENDOP_ADD);
    device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
    device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    device->SetTextureStageState(0, D3DTSS_RESULTARG, D3DTA_CURRENT);
    device->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);
    device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    device->SetSamplerState(0, D3DSAMP_MAXMIPLEVEL, 0);
}

void RenderPanel(IDirect3DDevice9* device, IDirect3DSurface9* backBuffer)
{
    SavedDeviceState saved;
    if (!saved.Capture(device))
        return;
    const bool ownScene = SUCCEEDED(device->BeginScene());
    device->SetRenderTarget(0, backBuffer);
    for (DWORD i = 1; i < kMaxRenderTargets; ++i)
        if (saved.ExtraTarget(i))
            device->SetRenderTarget(i, nullptr);
    device->SetDepthStencilSurface(nullptr);
    SetStateImGuiLeavesAlone(device);
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    if (ownScene)
        device->EndScene();
    saved.Restore(device);
}

void DrawOverlayFrame(IDirect3DDevice9* device)
{
    if (!GlobalConfig().Get().overlay)
    {
        SetVisible(false);
        return;
    }
    if (device->TestCooperativeLevel() != D3D_OK)
        return;
    IDirect3DSurface9* backBuffer = nullptr;
    if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)))
        return;
    D3DSURFACE_DESC desc = {};
    backBuffer->GetDesc(&desc);
    g_overlay.backBufferWidth = desc.Width;
    g_overlay.backBufferHeight = desc.Height;
    BuildPanelFrame(desc);
    RenderPanel(device, backBuffer);
    backBuffer->Release();
}

void DrawOverlayGuarded(IDirect3DDevice9* device)
{
    __try
    {
        DrawOverlayFrame(device);
    }
    __except (OverlayFaultFilter(GetExceptionCode(), "frame"))
    {
        g_overlay.failed = true;
        g_overlay.visible = false;
    }
}

void ShutDownImGui()
{
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}
}

void AttachOverlay(IDirect3DDevice9* device, HWND window)
{
    if (g_overlay.failed || !device || !window)
        return;
    if (g_overlay.device)
        DetachOverlay(g_overlay.device);
    if (!ChainOverlayWindowProc(window))
    {
        VF_LOG_ERROR("overlay: window %p could not be subclassed; overlay off", static_cast<void*>(window));
        return;
    }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ConfigureImGui();
    if (!ImGui_ImplWin32_Init(window))
    {
        ImGui::DestroyContext();
        RestoreClientWindowProc();
        VF_LOG_ERROR("overlay: the ImGui Win32 backend failed to start; overlay off");
        return;
    }
    ImGui_ImplDX9_Init(device);
    g_overlay.device = device;
    VF_LOG_INFO("overlay attached to window %p; %s opens it", static_cast<void*>(window),
                HotkeyName(GlobalConfig().Get().overlayKey).c_str());
}

void DetachOverlay(IDirect3DDevice9* device)
{
    if (!device || device != g_overlay.device)
        return;
    SetVisible(false);
    ShutDownImGui();
    const bool failed = g_overlay.failed;
    g_overlay = OverlayState();
    g_overlay.failed = failed;
    if (!RestoreClientWindowProc())
        VF_LOG_INFO("overlay detached; the window procedure stays chained because another hook follows it");
}

void ReleaseOverlayDeviceObjects(IDirect3DDevice9* device)
{
    if (device && device == g_overlay.device)
        ImGui_ImplDX9_InvalidateDeviceObjects();
}

void DrawOverlay(IDirect3DDevice9* device)
{
    if (g_overlay.visible && !g_overlay.failed && device && device == g_overlay.device)
        DrawOverlayGuarded(device);
}

bool OverlayVisible()
{
    return g_overlay.visible;
}
