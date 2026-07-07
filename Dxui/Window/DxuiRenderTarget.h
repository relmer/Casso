#pragma once

#include "Pch.h"
#include "Render/DxuiPainter.h"
#include "Render/DxuiTextRenderer.h"


class IDxuiTheme;




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRenderTarget
//
//  Abstract render-target base: it owns the D3D11 device + immediate
//  context and the DxuiPainter / DxuiTextRenderer pair, and drives the
//  per-frame render + present pump. A concrete subclass binds the pump to a
//  concrete output surface -- DxuiHwndSource binds it to an HWND (composition
//  or flip-discard) swap chain. This mirrors the split every mature
//  retained-mode DX stack uses:
//
//      * WPF          CompositionTarget      <- HwndTarget
//      * Direct2D     ID2D1RenderTarget      <- ID2D1HwndRenderTarget
//                                            <- ID2D1BitmapRenderTarget (offscreen)
//      * Skia         SkSurface (window- vs GPU/offscreen-backed)
//      * Qt           QPaintDevice           <- QWidget / QImage / FBO
//
//  The base owns the device + drawing pipeline; the subclass supplies the
//  swap chain, the back-buffer RTV, and the Present mechanics (matching D2D,
//  where the HWND-bound target lives in ID2D1HwndRenderTarget, not the base).
//  Naming follows WPF's *Target: the surface a control tree renders onto,
//  distinct from DxuiHwndSource's window-hosting role (NC hit-testing,
//  caption, popups, message routing). The device + painter members are
//  protected because the subclass creates / tears them down (it owns the
//  adapter + swap-chain details); the base only consumes them each frame.
//
//  Opt-in compose (FR-130). By default RenderFrame paints the content tree
//  straight to the subclass's back buffer -- byte-for-byte the legacy path,
//  zero extra cost, no offscreen allocation. When a consumer installs a
//  compose hook, RenderFrame instead paints the content to an OFFSCREEN
//  texture and hands the hook that texture's SRV plus the back-buffer RTV, so
//  it can post-process the whole frame (Gaussian blur, a see-through emulator
//  reveal, ...) before Present. The Settings live-preview compositor installs
//  it; every other window leaves the hook null and never allocates the target.
//  (The offscreen path is wired in a later slice; slice 1 extracts the pump.)
//
//  UI-thread only (FR-083), same as DxuiHwndSource.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiRenderTarget
{
public:
    DxuiRenderTarget          ();
    virtual ~DxuiRenderTarget ();

    //  Compose hook (opt-in). Receives the content tree already rendered to an
    //  offscreen texture (contentSrv) and the frame's final back-buffer RTV; it
    //  produces the presented frame (e.g. blur contentSrv + compose a
    //  see-through region over it). A non-null hook switches RenderFrame onto
    //  the offscreen path; null reverts to painting straight to the back buffer
    //  and releases the offscreen target.
    using ComposeHook = std::function<void (ID3D11ShaderResourceView * contentSrv,
                                            ID3D11RenderTargetView   * backBufferRtv,
                                            int                        widthPx,
                                            int                        heightPx)>;
    void  SetComposeHook (ComposeHook hook);
    bool  HasComposeHook () const { return static_cast<bool> (m_composeHook); }

protected:
    //  Render one frame: clear, run the before-present hook, paint the content
    //  (subclass PaintContent), then run the after-paint hook (or, on the
    //  offscreen path, the compose hook), and Present (subclass). No-op until a
    //  back buffer + painter are available. Called by the subclass's paint pump.
    void     RenderFrame          (const IDxuiTheme * theme);

    //  Drop the offscreen compose target (content texture / RTV / SRV). The
    //  subclass calls this on resize so RenderFrame reallocates at the new size.
    void     ReleaseComposeTarget ();

    //  ---- Subclass surface contract -------------------------------------
    //  The subclass owns the concrete output; RenderFrame drives these.

    //  The RTV the finished frame must land on (the swap-chain back buffer),
    //  and its pixel size. Null RTV => RenderFrame no-ops.
    virtual ID3D11RenderTargetView *  BackBufferRtv     () const = 0;
    virtual SIZE                      BackBufferSizePx  () const = 0;

    //  Paint the content tree (root panel, host caption, modal overlay) into
    //  `target` at the given pixel size, using the base painter / text renderer
    //  (m_painter / m_textRenderer). Called between clear and compose/present.
    virtual void  PaintContent  (ID3D11RenderTargetView * target,
                                 int                      widthPx,
                                 int                      heightPx,
                                 const IDxuiTheme       & theme) = 0;

    //  Present the composed frame (swap-chain Present + any DComp commit).
    virtual void  PresentFrame  () = 0;

    //  The back buffer as an IDXGISurface (for the Direct2D text target) and the
    //  target DPI. Used to (re)bind the text renderer's D2D target between the
    //  back buffer and the offscreen content texture on the compose path.
    virtual ComPtr<IDXGISurface>  BackBufferSurface () const = 0;
    virtual UINT                  TargetDpi         () const = 0;

    //  Rebind the text renderer's Direct2D target to the offscreen content
    //  texture (offscreen == true, compose path) or the swap-chain back buffer
    //  (offscreen == false, direct path). The subclass calls this from
    //  CreateBackBufferRtv so a resize rebinds to whichever target is live.
    HRESULT  BindTextTarget (bool offscreen);

    //  Legacy full-frame hooks the subclass exposes to consumers. The base
    //  reads them each RenderFrame: the before-present hook fills the back
    //  buffer before content paints (emulator composite); the after-paint hook
    //  post-processes the back buffer when NOT on the offscreen compose path.
    //  Return an empty std::function to skip.
    virtual const std::function<void()> &  BeforePresentHook () const = 0;
    virtual const std::function<void(ID3D11RenderTargetView *, int, int)> &  AfterPaintHook () const = 0;

    //  Device + drawing pipeline. Created + torn down by the subclass (it owns
    //  the adapter / swap-chain details); consumed by RenderFrame.
    ComPtr<ID3D11Device>              m_device;
    ComPtr<ID3D11DeviceContext>       m_context;
    std::unique_ptr<DxuiPainter>      m_painter;
    std::unique_ptr<DxuiTextRenderer> m_textRenderer;

private:
    HRESULT  EnsureComposeTarget (int widthPx, int heightPx, bool & recreated);

    //  Offscreen compose target (opt-in). Allocated lazily by EnsureComposeTarget
    //  the first frame a compose hook is installed, sized to the back buffer;
    //  released on resize / hook-clear.
    ComPtr<ID3D11Texture2D>           m_contentTex;
    ComPtr<ID3D11RenderTargetView>    m_contentRtv;
    ComPtr<ID3D11ShaderResourceView>  m_contentSrv;
    int                               m_contentWidthPx  = 0;
    int                               m_contentHeightPx = 0;

    //  Tracks whether the text renderer's D2D target is currently bound to the
    //  offscreen content texture (true) or the back buffer (false), so
    //  RenderFrame only rebinds when the target actually needs to change.
    bool                              m_textBoundToOffscreen = false;

    ComposeHook                       m_composeHook;
};
