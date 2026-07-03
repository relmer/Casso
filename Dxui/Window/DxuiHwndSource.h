#pragma once

#include "Pch.h"
#include "Core/DxuiPanel.h"
#include "Core/DxuiFocusManager.h"
#include "Core/DxuiDpiScaler.h"
#include "Render/DxuiPainter.h"
#include "Render/DxuiTextRenderer.h"


class DxuiPopupHost;
class IDxuiHostClient;
class DxuiCaptionBar;



////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHwndSource
//
//  Top-level Win32 host window that owns the HWND, the D3D11 device,
//  the DXGI swap chain, the painter / text renderer pair, and the
//  root DxuiPanel for one custom-chromed application window.
//
//  Construction modes:
//      Full ownership (default ctor + Create) — registers a window
//          class, calls CreateWindowEx, owns + tears down everything.
//          This is the Phase 7 surface.
//      Test mode (synthetic-root ctor) — no HWND, no device, no swap
//          chain. Lets unit tests drive ClassifyHitForTest() against
//          a hand-built root panel. See the synthetic-root ctor
//          overload below.
//
//      Adopt mode (CreateInAdoptMode) — wraps an existing HWND
//          whose lifecycle, swap chain, and D3D device the caller
//          continues to own. The host classifies NC hits (with an
//          optional caller-supplied hit-test delegate) and tracks
//          DPI / theme for its internal panel tree. The caller's
//          WndProc forwards each message through HandleMessage and
//          uses the bool return value to decide whether to return
//          immediately or fall through to the legacy logic.
//
//  WM_NCHITTEST classification order:
//      1. The eight resize edges, sized off CreateParams::
//         resizeBorderDip and the current DPI.
//      2. Front-to-back walk of the root panel tree; the first
//         visible IDxuiControl whose bounds contain the point gets
//         to ClassifyHit(). DxuiHitTestKind::MaxButton translates to
//         HTMAXBUTTON so Win11 fires the snap-layouts popover.
//
//  All public methods are called on the UI thread (FR-083); every
//  public entry asserts this in debug builds.
//
////////////////////////////////////////////////////////////////////////////////



enum class DxuiHwndSourceBackdrop
{
    None,
    Mica,
};



//
//  Host-owned caption style. When not None, DxuiHwndSource builds and
//  owns an internal DxuiCaptionBar (gradient + app icon + title text +
//  system buttons) at the top of the client area, classifies / paints /
//  routes it itself, and exposes it to the consumer only through a
//  SetWindowText-like API (SetTitle / SetCaptionIcon). The consumer
//  never adopts or positions a caption object. Standard = min/max/close;
//  CloseOnly = a single close button (fixed-size dialogs / tool windows).
//
enum class DxuiCaptionStyle
{
    None,
    Standard,
    CloseOnly,
};



class DxuiHwndSource
{
public:
    struct CreateParams
    {
        std::wstring             title;
        HINSTANCE                hInstance        = nullptr;
        HWND                     ownerHwnd        = nullptr;
        bool                     borderless       = true;
        bool                     resizable        = true;
        bool                     roundedCorners   = true;
        bool                     darkMode         = true;
        DxuiHwndSourceBackdrop   backdrop         = DxuiHwndSourceBackdrop::Mica;
        float                    resizeBorderDip  = 6.0f;
        SIZE                     initialSizeDip   = { 1024, 768 };

        // Optional class-name override. When non-null, the host
        // registers its WNDCLASS under this name instead of the
        // auto-generated per-instance "DxuiHwndSource_<serial>_<ptr>"
        // string. Lets a consumer keep a stable, identifiable class
        // name (e.g. "CassoWindow") for tooling / Spy++ / window
        // enumeration / regression-tracking purposes.
        LPCWSTR                  classNameOverride        = nullptr;

        // Optional pre-computed window-pixel placement. When
        // useInitialWindowRectPx is true, Create() passes the rect's
        // (left, top, right-left, bottom-top) directly to
        // CreateWindowExW and SKIPS the initialSizeDip → pixel
        // conversion. Lets the caller restore a saved placement
        // verbatim, or pre-compute frame-inclusive dimensions via
        // AdjustWindowRectExForDpi against a bespoke style mask.
        bool                     useInitialWindowRectPx   = false;
        RECT                     initialWindowRectPx      = {};

        // Optional app icons. When non-null, applied to the HWND
        // after CreateWindowEx via WM_SETICON. ICON_BIG drives the
        // Alt-Tab and taskbar icon plus Win32 MessageBox dialog
        // titles; ICON_SMALL drives the caption icon. Caller retains
        // ownership and lifetime — typically these are LR_SHARED
        // HICONs from LoadImageW so no cleanup is needed.
        HICON                    appIconBig               = nullptr;
        HICON                    appIconSmall             = nullptr;

        // When true (the default), Create() stands up a D3D11 device
        // + DXGI flip-discard swap chain on the host HWND so the
        // internal panel tree paints through GetSwapChain() /
        // GetBackBufferRtv(). Set false when the consumer renders
        // through its own swap chain (e.g. a child render-surface
        // HWND) and does not want Dxui creating an unused swap chain
        // on the parent. With this flag false, GetDevice() /
        // GetContext() / GetSwapChain() / GetBackBufferRtv() all
        // return nullptr and the consumer is responsible for its own
        // rendering pipeline.
        bool                     createSwapChain          = true;

        // Host-owned caption (the SetWindowText model). When not None,
        // Create() builds an internal DxuiCaptionBar at the top of the
        // client area; the consumer drives it only via SetTitle /
        // SetCaptionIcon and lays its own content out below
        // CaptionHeightPx(). Default None preserves the legacy path
        // where the consumer adopts its own title-bar control.
        DxuiCaptionStyle         captionStyle             = DxuiCaptionStyle::None;

        // When true (and a host caption exists), the host lays its
        // content root out in the client region BELOW the caption strip
        // instead of the full client rect. Use for content-hosting
        // windows (dialogs, tool windows) whose root should not slide
        // under the caption. Default false preserves the full-bleed
        // behavior the main window relies on (it composites the emulator
        // viewport under an overlay caption).
        bool                     insetRootBelowCaption    = false;
    };


    DxuiHwndSource  ();
    ~DxuiHwndSource ();

    HRESULT  Create            (const CreateParams & params);
    void     Destroy           ();

    //
    //  Adopt mode — wrap an existing HWND that the caller continues
    //  to own. The host does NOT call CreateWindow / DestroyWindow
    //  and does NOT create a swap chain or D3D device; the caller's
    //  existing rendering pipeline runs unmodified. The caller's
    //  WndProc forwards messages through HandleMessage(); the host
    //  classifies NC hits (via the optional SetHitTestDelegate
    //  plug-in) and propagates DPI / theme to its internal panel
    //  tree. Tests may pass nullptr for existingHwnd to drive the
    //  classifier without standing up a real top-level window.
    //
    static HRESULT  CreateInAdoptMode  (HWND                              existingHwnd,
                                        const CreateParams              & params,
                                        std::unique_ptr<DxuiHwndSource> & outHost);

    //
    //  Adopt-mode hit-test plug-in. The delegate is invoked from
    //  WM_NCHITTEST with the original screen-space mouse position
    //  before the framework's resize-edge / panel-tree classifier
    //  runs. Returning anything other than HTNOWHERE wins. Lets a
    //  consumer plug in its bespoke caption / system-button hit-
    //  testing without first reshaping its chrome onto DxuiCaptionBar.
    //
    void  SetHitTestDelegate  (std::function<LRESULT (POINT ptScreen)> delegate);

    //
    //  Public WndProc forwarder for adopt-mode consumers. Returns
    //  true when Dxui owns the message end-to-end (caller returns
    //  outResult immediately); returns false to let the caller's
    //  WndProc keep handling it (and ultimately fall through to
    //  DefWindowProc). Routes WM_NCCALCSIZE / WM_NCHITTEST through
    //  Dxui; observes WM_DPICHANGED / WM_SETTINGCHANGE /
    //  WM_THEMECHANGED / WM_DWMCOLORIZATIONCOLORCHANGED for tree-
    //  side DPI + theme propagation without claiming the message.
    //
    bool  HandleMessage  (UINT msg, WPARAM wp, LPARAM lp, LRESULT & outResult);

    HWND          Hwnd          () const { return m_hwnd; }
    DxuiPanel  &  Root          ()       { return *RootPanel(); }
    const DxuiDpiScaler &  Scaler  () const { return m_scaler; }
    void          SetTheme      (const IDxuiTheme * theme);

    //
    //  Host-owned caption (SetWindowText model). Active only when the
    //  window was created with CreateParams::captionStyle != None.
    //  SetTitle drives both the Win32 window text (so Alt-Tab / taskbar
    //  stay correct) and the internal DxuiCaptionBar; SetCaptionIcon
    //  feeds the caption's app-icon glyph (premultiplied BGRA, as from
    //  LoadImage + a DIB blit). CaptionHeightPx returns the reserved
    //  top-strip height in physical pixels (0 when no host caption) so
    //  the consumer can lay its own content out beneath it.
    //
    void          SetTitle        (const std::wstring & title);
    void          SetCaptionIcon  (std::vector<uint32_t> bgraPremul, int widthPx, int heightPx);
    int           CaptionHeightPx () const;

    //
    //  Adopt-mode caption hooks. A full-ownership host paints + lays out
    //  + routes the caption itself; an adopt-mode host owns no paint pump
    //  and no NC-mouse stream, so the consumer drives these from its own
    //  render loop / WndProc: RenderCaption inside its paint pass,
    //  LayoutCaptionForClient on WM_SIZE. Both no-op when there is no
    //  host caption. (Caption NC-mouse routing is handled inside
    //  HandleMessage when a caption exists.)
    //
    void          RenderCaption          (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);
    void          LayoutCaptionForClient (const RECT & clientPx);

    //
    //  Replace the host's root panel with a caller-supplied panel.
    //  Lets a consumer install a fully-assembled content tree (e.g.
    //  a DxuiDialog or a SettingsWindow content panel) as the host's
    //  paint / hit-test / focus / accessibility root without going
    //  through `Root().Add<...>()` piece-by-piece. The previous root
    //  (and everything under it) is destroyed.
    //
    //  When `m_hwnd` already exists the new panel's bounds are
    //  recomputed from the current client rect so it lays out
    //  immediately; otherwise the next WM_SIZE / Create() drives
    //  layout. In synthetic mode the panel inherits the previous
    //  root's bounds.
    //
    //  Passing nullptr asserts in debug; release builds silently
    //  drop the call. (The host always has a root panel.)
    //
    void          SetContentPanel (std::unique_ptr<DxuiPanel> panel);

    //
    //  Install a NON-owning content root. Unlike SetContentPanel (which
    //  takes ownership), this points the host's paint / layout / focus /
    //  hit-test / accessibility pump at an externally-owned DxuiPanel --
    //  used by DxuiWindow, which IS its own content root (it derives from
    //  DxuiPanel) while owning this host as its backend. When set, the ref
    //  shadows the owned root everywhere the pump consumes the root; the
    //  owned root is left intact but unused. Passing nullptr reverts to the
    //  owned root. When `m_hwnd` exists the ref is laid out + focus-attached
    //  immediately from the current client rect.
    //
    void          SetContentRootRef (DxuiPanel * root);

    //
    //  Convenience wrappers around `::SetTimer` / `::KillTimer` for
    //  consumers that want WM_TIMER ticks dispatched through
    //  `IDxuiHostClient::OnTimer`. Both methods assert the host
    //  owns a real HWND (full-ownership mode after Create()); in
    //  synthetic / adopt-without-HWND modes they no-op in release.
    //  Tests should drive `OnTimer` directly rather than relying
    //  on the OS timer queue.
    //
    bool          SetTimer       (UINT_PTR timerId, UINT intervalMs);
    bool          KillTimer      (UINT_PTR timerId);

    //
    //  Install an optional client object that receives the Win32
    //  messages the host does not own end-to-end (commands,
    //  keyboard input, painting, timers, ...). The host stores a
    //  non-owning pointer; the client must outlive the host or
    //  call SetClient(nullptr) before destruction. Passing
    //  nullptr clears any previously-installed client.
    //
    void          SetClient     (IDxuiHostClient * client);
    void          SetDefaultProcForTest      (std::function<LRESULT (HWND, UINT, WPARAM, LPARAM)> defaultProc);
    void          SetTrackMouseEventForTest  (std::function<BOOL (TRACKMOUSEEVENT *)> trackMouseEvent);

    //
    //  Test seams for the paint-pump-ownership gate. The host drives
    //  the root panel's layout on resize / DPI change ONLY when it
    //  owns its paint pump (full-ownership mode with a live swap
    //  chain). In adopt mode -- or full-ownership mode created with
    //  createSwapChain = false -- the consumer owns chrome layout and
    //  the host must not run a second, competing layout pass.
    //
    void          SetOwnsPaintPumpForTest    (bool ownsPaintPump) { m_ownsPaintPump = ownsPaintPump; }
    void          RelayoutRootForTest        (const RECT & clientPx) { MaybeRelayoutRoot (clientPx); }

    //
    //  Shared-device accessors. Full-ownership mode creates the D3D11
    //  device, immediate context, and DXGI flip-discard swap chain on
    //  Create(); these accessors return non-owning pointers so a
    //  consumer (e.g., the Apple ][ framebuffer renderer) can draw
    //  into the same swap chain rather than standing up its own.
    //  Lifetime is bounded by the matching Destroy() call.
    //
    //  In adopt mode and synthetic mode these all return nullptr —
    //  the caller continues to own its own device + swap chain.
    //  Tests must not call these in test mode.
    //
    //  GetBackBufferRtv() returns the host's back-buffer render-target
    //  view, recreated automatically on every WM_SIZE. Consumers that
    //  composite via the BeforePresentHook should bind this RTV (or
    //  one derived from it) themselves; the host's paint pump rebinds
    //  it before walking the panel tree but does NOT guarantee any
    //  particular OM state on hook entry.
    //
    ID3D11Device         *  GetDevice          () const { return m_device.Get();    }
    ID3D11DeviceContext  *  GetContext         () const { return m_context.Get();   }
    IDXGISwapChain1      *  GetSwapChain       () const { return m_swapChain.Get(); }
    ID3D11RenderTargetView * GetBackBufferRtv  () const { return m_rtv.Get();       }
    // Host-owned text renderer; null in adopt / synthetic mode (host owns no paint pump).
    IDxuiTextRenderer    *  GetTextRenderer    () const { return m_textRenderer.get(); }

    //
    //  Optional before-present hook. Installed by a consumer (e.g.
    //  the Apple ][ framebuffer renderer) that wants to composite
    //  into the host's swap-chain back buffer BEFORE the standard
    //  panel-tree Paint pump runs -- so the chrome paints on top of
    //  the composited content -- and before the host calls Present.
    //  The hook owns a full-buffer write; the panel-tree painter /
    //  text passes are additive and do not clear the RTV. The host
    //  stores the callback; it is invoked once per frame from the
    //  host's WM_PAINT pump. Passing a null function clears any
    //  previously-installed hook.
    //
    void  SetBeforePresentHook  (std::function<void()> hook);
    const std::function<void()> &  BeforePresentHook  () const { return m_beforePresentHook; }

    LRESULT  WndProc           (UINT msg, WPARAM wp, LPARAM lp);

    //
    //  Popup pool. Acquire returns an initialized DxuiPopupHost
    //  ready for Show(); Release returns it to the LIFO pool for
    //  reuse. The pool grows on demand (initial size 3). Debug
    //  builds expose hit / miss counters for reuse verification.
    //
    DxuiPopupHost  *  AcquirePopup ();
    void              ReleasePopup (DxuiPopupHost * popup);

    //
    //  Adopt-mode popup device. A full-ownership host seeds its popup
    //  pool from its own m_device/m_context, but an adopt-mode host
    //  (it wraps a foreign HWND and owns no device) has none — its
    //  consumer's renderer does. The consumer calls this after
    //  CreateInAdoptMode so pooled popups can create real swap chains
    //  instead of falling back to headless test mode. Must be called
    //  before the first AcquirePopup(); the device/context must outlive
    //  every popup (close all popups before the renderer is torn down).
    //
    void              SetPopupRenderDevice (ID3D11Device         * device,
                                            ID3D11DeviceContext  * context);

#ifdef _DEBUG
    size_t  PopupHits   () const { return m_popupHits;   }
    size_t  PopupMisses () const { return m_popupMisses; }
    size_t  PopupPoolSize () const { return m_popupPool.size(); }
    size_t  PopupActiveCount () const { return m_popupActive.size(); }
#endif

#ifdef _DEBUG
    //
    //  Debug-build instrumentation seam — records which DwM knobs were
    //  applied during the last Create(). Lets unit tests / integration
    //  smoke checks verify all four CreateParams DwM bits round-trip
    //  without poking at the live HWND.
    //
    struct DwmAppliedSeam
    {
        bool                    roundedCornersApplied = false;
        bool                    micaBackdropApplied   = false;
        bool                    darkModeApplied       = false;
        bool                    extendFrameApplied    = false;
        DxuiHwndSourceBackdrop  backdropRequested     = DxuiHwndSourceBackdrop::None;
        bool                    roundedRequested      = false;
        bool                    darkRequested         = false;
    };

    const DwmAppliedSeam &  DwmSeam  () const { return m_dwmSeam; }
#endif

    //
    //  Synthetic-root constructor for unit tests. No HWND, no device,
    //  no swap chain. Caller hands over an in-memory root panel
    //  (typically containing DxuiCaptionBar + DxuiSystemButton
    //  children with hand-set bounds) and tests drive ClassifyHitForTest
    //  to assert the NC classification falls out correctly.
    //
    DxuiHwndSource (RECT                         clientBoundsDip,
                    float                        resizeBorderDip,
                    std::unique_ptr<DxuiPanel>   root);

    //
    //  Test seam — runs the NC hit-test classifier against a synthetic
    //  client-coordinate point and returns the raw DxuiHitTestKind
    //  (before mapping to HT*). Available in both ownership modes.
    //
    DxuiHitTestKind  ClassifyHitForTest  (POINT clientDip) const;

    //
    //  Static helper — maps a DxuiHitTestKind to the matching Win32
    //  HT* code (HTCLIENT, HTCAPTION, HTMINBUTTON, HTMAXBUTTON,
    //  HTCLOSE, HTLEFT, HTRIGHT, HTTOP, HTBOTTOM, HTTOPLEFT,
    //  HTTOPRIGHT, HTBOTTOMLEFT, HTBOTTOMRIGHT). Exposed publicly so
    //  unit tests can assert the final NC return without instantiating
    //  the full WndProc dispatch.
    //
    static LRESULT  KindToHt  (DxuiHitTestKind kind);

private:
    static LRESULT CALLBACK  s_WndProcThunk   (HWND, UINT, WPARAM, LPARAM);

    HRESULT  CreateDeviceAndSwapChain  ();
    HRESULT  CreateRenderResources     ();
    void     ReleaseRenderResources    ();
    HRESULT  CreateBackBufferRtv       ();
    void     ReleaseBackBufferRtv      ();
    void     PaintPump                 ();
    void     ApplyDwmConfiguration     ();

    LRESULT  HandleNcCalcSize          (WPARAM wp, LPARAM lp);
    LRESULT  HandleNcHitTest           (LPARAM lp);
    LRESULT  HandleNcMouse             (UINT msg, WPARAM wp, LPARAM lp);
    LRESULT  DefaultProc               (UINT msg, WPARAM wp, LPARAM lp);
    BOOL     TrackMouseEventHost       (TRACKMOUSEEVENT * pEvent);
    void     TrackClientMouseLeave     ();
    void     DispatchNcUpToTrackedButton (LPARAM lp);
    void     HandleDpiChanged          (WPARAM wp, LPARAM lp);
    void     HandleSize                (WPARAM wp, LPARAM lp);
    void     HandleThemeChange         ();
    void     MaybeRelayoutRoot         (const RECT & clientPx);
    DxuiPanel *  RootPanel             () const { return m_rootRef != nullptr ? m_rootRef : m_root.get(); }
    void     LayoutCaption             (const RECT & clientDip);
    void     BuildCaption              ();
    bool     RouteCaptionNcMouse       (UINT msg, WPARAM wp, LPARAM lp);

    DxuiHitTestKind  ClassifyHitInternal       (POINT clientDip, RECT clientBoundsDip) const;
    IDxuiControl   *  FindNcSystemControlAt    (POINT clientDip) const;
    void              NotifySystemButtonsMaximized (bool maximized);
    void              InitializePooledPopup    (DxuiPopupHost * popup);

    static DxuiHitTestKind  ClassifyResizeEdge (POINT clientDip,
                                                RECT  clientBoundsDip,
                                                int   resizeBorderPx);

    static void            NotifySystemButtonsMaximizedInTree (IDxuiControl * control, bool maximized);
    static IDxuiControl *  FindNcSystemControlInTree          (IDxuiControl * control, POINT clientDip);


    static constexpr int     s_kMinResizeBorderPx    = 4;
    static constexpr UINT    s_kDefaultDpi           = 96;
    static constexpr LONG    s_kExtendFrameInsetPx   = 1;
    static constexpr size_t  s_kPopupPoolInitialSize = 3;

    // Distinct per-instance window class names — every Create()
    // generates a fresh class so multiple host windows in one
    // process don't share registration state.
    static inline std::atomic<uint32_t>  s_classSerial { 0 };


    HWND                              m_hwnd               = nullptr;
    HINSTANCE                         m_hInstance          = nullptr;
    std::wstring                      m_className;
    CreateParams                      m_params;
    DxuiDpiScaler                     m_scaler;

    ComPtr<ID3D11Device>              m_device;
    ComPtr<ID3D11DeviceContext>       m_context;
    ComPtr<IDXGISwapChain1>           m_swapChain;
    ComPtr<ID3D11RenderTargetView>    m_rtv;

    std::unique_ptr<DxuiPainter>      m_painter;
    std::unique_ptr<DxuiTextRenderer> m_textRenderer;
    std::unique_ptr<DxuiPanel>        m_root;
    DxuiPanel *                       m_rootRef            = nullptr;
    std::unique_ptr<DxuiCaptionBar>   m_caption;
    DxuiFocusManager                  m_focusManager;
    const IDxuiTheme *                m_theme              = nullptr;

    bool                              m_ownsHwnd           = false;
    bool                              m_ownsPaintPump      = false;
    bool                              m_synthetic          = false;
    bool                              m_adoptMode          = false;
    bool                              m_classRegistered    = false;

    IDxuiHostClient *                 m_client             = nullptr;

    std::function<LRESULT (POINT)>                         m_hitTestDelegate;
    std::function<void()>                                  m_beforePresentHook;
    std::function<LRESULT (HWND, UINT, WPARAM, LPARAM)>    m_defaultProcForTest;
    std::function<BOOL (TRACKMOUSEEVENT *)>                m_trackMouseEventForTest;
    IDxuiControl *                                         m_lastHoveredNcControl = nullptr;
    bool                                                   m_clientMouseLeaveTracking = false;

    // Popup pool (FR-055). Initial size 3; grows on demand. m_popupPool
    // holds LIFO-available instances; m_popupActive holds currently
    // checked-out instances so the host can forward broadcast events
    // (WM_DPICHANGED_BEFOREPARENT) to live popups.
    std::vector<std::unique_ptr<DxuiPopupHost>>  m_popupPool;
    std::vector<DxuiPopupHost *>                 m_popupActive;

    // Adopt-mode popup device/context (non-owning). Null in
    // full-ownership mode (m_device/m_context are used instead).
    ComPtr<ID3D11Device>                         m_popupDevice;
    ComPtr<ID3D11DeviceContext>                  m_popupContext;
#ifdef _DEBUG
    size_t                                       m_popupHits   = 0;
    size_t                                       m_popupMisses = 0;
#endif

#ifdef _DEBUG
    DwmAppliedSeam                    m_dwmSeam;
#endif
};
