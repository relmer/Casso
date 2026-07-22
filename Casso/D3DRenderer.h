#pragma once

#include "Pch.h"

#include "CrtPostProcess.h"





////////////////////////////////////////////////////////////////////////////////
//
//  D3DRenderer
//
////////////////////////////////////////////////////////////////////////////////

class D3DRenderer
{
public:
    D3DRenderer();
    ~D3DRenderer();

    // Adopts an externally-owned device / context / swap chain (the
    // host's, typically DxuiHwndSource's) rather than creating its
    // own. Builds this renderer's upload texture, sampler, shaders,
    // and vertex / index buffers plus the CRT post-process chain, but
    // holds NO back-buffer RTV of its own -- it composites into the
    // host's RTV passed to UploadAndComposite, which the caller invokes
    // once per frame; the host owns Present.
    //
    // `initialTargetRect` is the pixel-space rectangle inside the
    // back buffer where the Apple ][ framebuffer should composite;
    // EmulatorShell drives this from the DxuiViewport bounds.
    HRESULT Initialize (ID3D11Device          * pDevice,
                        ID3D11DeviceContext   * pContext,
                        IDXGISwapChain1       * pSwapChain,
                        int                     texWidth,
                        int                     texHeight,
                        const RECT            & initialTargetRect);

    // Uploads the framebuffer and runs the CRT post-process pass,
    // writing into the caller-supplied `dstRtv` (the host's back-buffer
    // RTV). Skips the swap-chain Present -- the host's paint pump owns
    // it. Does NOT clear the full back buffer (the host cleared it and
    // the CRT final pass overwrites it) and skips any after-blit chrome
    // hook (chrome paints via the host's panel-tree walk afterward).
    HRESULT UploadAndComposite (ID3D11RenderTargetView * dstRtv, const uint32_t * framebuffer);

    HRESULT ToggleFullscreen (HWND hwnd);

    // Live-wire path for CRT params (brightness, scanlines, bloom,
    // color-bleed). EmulatorShell pushes a fresh `CrtParams` once per UI
    // frame (right before UploadAndComposite) so slider edits from the
    // Settings panel land on the next-rendered frame without ever
    // pausing the emulator (FR-041). Outside the live emulator path
    // (e.g., tests, headless boot) the field stays at its in-struct
    // defaults so the renderer behaves like a passthrough.
    void SetCrtParams    (const CrtParams & params) { m_crtParams     = params; }

    // Pixel-space rectangle inside the host swap-chain back buffer
    // where the Apple ][ framebuffer should composite. EmulatorShell
    // pushes a fresh rect from OnViewportBoundsChanged whenever the
    // DxuiViewport child of the host's root panel reports new bounds.
    // The renderer consumes the rect in the host-swap-chain composite
    // path (UploadAndComposite) to position the emulator content.
    void SetTargetBounds  (const RECT & boundsPx)   { m_targetBoundsPx = boundsPx; }
    RECT GetTargetBounds  () const                  { return m_targetBoundsPx; }

    // Push the host's current back-buffer pixel dimensions so the CRT
    // post-process sizes its intermediate render targets and the
    // aspect-fit math correctly. The host owns ResizeBuffers; the
    // renderer never resizes the swap chain, so it learns the new size
    // through this setter from EmulatorShell::OnSize.
    void SetBackBufferSize (int widthPx, int heightPx)
    {
        m_backBufferW         = std::max (0, widthPx);
        m_backBufferH         = std::max (0, heightPx);
        m_physicalBackBufferW = std::max (m_physicalBackBufferW, m_backBufferW);
        m_physicalBackBufferH = std::max (m_physicalBackBufferH, m_backBufferH);

        // A size change (called from EmulatorShell::OnSize) invalidates the
        // just-resized back buffer, so the idle-present short-circuit in
        // NeedsPresent must not suppress this frame -- otherwise a resize
        // that doesn't also dirty the framebuffer or CRT params leaves the
        // discarded buffer on screen (a visible theme-background flash on
        // non-dark themes) until the next real change.
        m_redrawForced = true;
    }

    // Returns true if the next frame would produce a visually
    // different result than the last one we presented. The shell uses
    // this to short-circuit the entire 9-pass post-process when nothing
    // has changed -- otherwise we burn ~25%% GPU per refresh on a
    // static screen. `framebufferDirty` is the only thing the renderer
    // can't tell on its own; pass true whenever the emulator produced a
    // new frame.
    bool NeedsPresent     (bool framebufferDirty) const;
    void MarkRedrawNeeded ()                            { m_redrawForced = true; }

    bool IsFullscreen() const { return m_fullscreen; }

    // Accessors for the current swap chain dimensions so the
    // EmulatorShell can populate `CrtParams::outputW/H` for the shader
    // constant buffer without piping the window size through a side
    // channel.
    int  GetBackBufferWidth  () const { return m_backBufferW; }
    int  GetBackBufferHeight () const { return m_backBufferH; }
    RECT GetEmulatorContentScreenRect() const { return m_emulatorContentScreenRect; }

    void Shutdown();

    // Accessors for the underlying device. Both
    // return non-owning pointers whose lifetime is bounded by
    // Initialize -> Shutdown on this renderer.
    ID3D11Device        * GetDevice  () const { return m_device.Get  (); }
    ID3D11DeviceContext * GetContext() const { return m_context.Get(); }

private:
    HRESULT InitializeShaders();
    void    CacheEmulatorContentScreenRect (const RECT & fittedRect);

    // Post-device-adoption pipeline setup for Initialize: dynamic
    // upload texture, sampler, shader programs, vertex / index
    // buffers, CRT post-process chain. Assumes m_device, m_context,
    // m_swapChain are populated.
    HRESULT CreateRenderResources (int texWidth, int texHeight);

    // Aspect-fits the emulator content into `contentRect`, caches the
    // resulting on-screen rect, and runs the CRT post-process pass into
    // `dstRtv`. Timed as "D3DRenderer.CrtPostProcess".
    HRESULT RenderCrtFrame (ID3D11RenderTargetView * dstRtv, const RECT & contentRect);

    ComPtr<ID3D11Device>             m_device;
    ComPtr<ID3D11DeviceContext>      m_context;
    // IDXGISwapChain2 (rather than the base IDXGISwapChain) gives us
    // SetSourceSize, the flip-model mechanism for telling DWM what
    // sub-rect of the (oversized, fixed-allocation) back buffer to
    // present. SetSourceSize replaces ResizeBuffers on the hot drag
    // path; ResizeBuffers under driver stress was the source of the
    // DXGI_ERROR_DRIVER_INTERNAL_ERROR device-removed crashes.
    ComPtr<IDXGISwapChain2>          m_swapChain;
    ComPtr<ID3D11Texture2D>          m_texture;
    ComPtr<ID3D11ShaderResourceView> m_srv;
    ComPtr<ID3D11SamplerState>       m_sampler;
    ComPtr<ID3D11VertexShader>       m_vertexShader;
    ComPtr<ID3D11PixelShader>        m_pixelShader;
    ComPtr<ID3D11Buffer>             m_vertexBuffer;
    ComPtr<ID3D11Buffer>             m_indexBuffer;
    ComPtr<ID3D11InputLayout>        m_inputLayout;

    // CRT post-process pass. Owns the intermediate ping-pong RTs +
    // the per-effect HLSL pixel shaders. Initialized in Initialize once
    // the device is up; torn down in Shutdown. `Process` is invoked
    // unconditionally from RenderCrtFrame -- a disabled effect maps
    // to a zero magnitude in CrtParams, which the shaders treat as a
    // pass-through (see CrtPostProcess.cpp). The intermediate RTs are
    // resized lazily inside Process() when the back buffer size changes.
    CrtPostProcess                   m_crtPost;
    CrtParams                        m_crtParams;
    // Snapshot of the last successfully-presented CrtParams so
    // NeedsPresent() can tell when sliders / toggles really changed.
    CrtParams                        m_lastPresentedParams      = {};
    bool                             m_redrawForced             = true;
    // Counts how many frames have been presented since the emulator
    // last produced a new framebuffer. Used by NeedsPresent to stop
    // re-rendering after the persistence trail has finished decaying
    // (otherwise persistence>0 would force a present every vsync
    // forever, even on a fully static screen).
    int                              m_idleFramesSinceFbChange  = 0;
    // Logical (presented) size = current client area. All public
    // accessors and chrome layout consume this.
    int                              m_backBufferW         = 0;
    int                              m_backBufferH         = 0;
    // Physical (allocated) size of the flip-model back buffer.
    // Stays large to avoid ResizeBuffers on every drag tick; only
    // grown when a client area exceeds it (e.g., user dragged the
    // window onto a larger monitor than we initially sized for).
    int                              m_physicalBackBufferW = 0;
    int                              m_physicalBackBufferH = 0;

    // Pixel-space rectangle inside the host swap-chain back buffer
    // where the Apple ][ framebuffer should composite. Pushed in by
    // EmulatorShell whenever the DxuiViewport child of the host's
    // root panel reports new bounds. Consumed by the host-swap-chain
    // composite path (UploadAndComposite) to position the emulator
    // content within the back buffer.
    RECT                             m_targetBoundsPx      = {};

    int     m_texWidth         = 0;
    int     m_texHeight        = 0;
    bool    m_fullscreen       = false;
    bool    m_deviceRemoved    = false;

    // Windowed state captured on fullscreen entry. Full WINDOWPLACEMENT (not
    // just a rect) so a maximized window round-trips back to maximized with
    // its underlying normal size intact. m_fsTransition guards against nested
    // toggles: an assert/message-box modal loop pumping messages mid-
    // transition can dispatch a queued Alt+Enter, and a nested enter would
    // capture the borderless fullscreen state as the "windowed" state --
    // leaving an unescapable full-monitor popup on exit.
    WINDOWPLACEMENT  m_windowedPlacement  = {};
    LONG             m_windowedStyle      = 0;
    bool             m_fsTransition       = false;

    RECT    m_emulatorContentScreenRect   = {};
};




