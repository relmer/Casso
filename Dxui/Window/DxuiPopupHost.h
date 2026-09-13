#pragma once

#include "Pch.h"
#include "Core/DxuiPanel.h"
#include "Render/DxuiPainter.h"
#include "Render/DxuiTextRenderer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupHost
//
//  Owns a top-level WS_POPUP HWND with its own small DXGI composition
//  swap chain (CreateSwapChainForComposition + DirectComposition visual,
//  not CreateSwapChainForHwnd — popup HWNDs need DComp for proper
//  z-order, transparency, and shadow). Shares the parent DxuiHwndSource's
//  ID3D11Device (the device is non-owning).
//
//  Use cases (one popup host per active popup):
//      DxuiComboBox's option list                — placement Below /
//                                                  flip Above if
//                                                  off-screen
//      DxuiPopupMenu and its cascading submenus  — placement AtCursor;
//                                                  child submenus link
//                                                  through ParentPopup
//      DxuiTooltip text balloon                  — WS_EX_TRANSPARENT |
//                                                  WS_EX_LAYERED so
//                                                  clicks pass through
//      DxuiMenuBar submenu (Phase 10 consumer)   — owner chain rooted
//                                                  at the menubar item
//
//  Production lifecycle (Initialize + Show + Close):
//      Initialize() registers a per-instance window class, then on
//      Show() creates the HWND, swap chain, DComp visual, and renders
//      the content panel into it. Close() hides the HWND and resolves
//      the std::future returned from GetCompletion().
//
//  Test mode (InitializeForTest):
//      No HWND, no device, no swap chain. All placement, dismiss-
//      policy classification, chain bookkeeping, and pool acquire /
//      release behavior is exercised through pure state mutation
//      and the static ComputePlacementForTest / ShouldDismissForTest
//      seams.
//
//  All public methods are UI-thread-only (FR-083); each entry asserts
//  via DXUI_ASSERT_UI_THREAD().
//
////////////////////////////////////////////////////////////////////////////////


class DxuiHwndSource;
class IDxuiTheme;



enum class DxuiPopupPlacement
{
    Below,
    Above,
    Right,
    Left,
    AtCursor,
};



enum class DxuiPopupDismiss
{
    OnClickOutside,
    OnClickAnywhere,
    OnPointerLeave,
    Manual,
};



enum class DxuiPopupInput
{
    Interactive,
    PassThrough,
};



//
//  Classification of how a candidate mouse / pointer event relates to
//  the popup and its owner chain. Fed to ShouldDismissForTest to
//  decide whether the configured DxuiPopupDismiss policy should fire.
//
enum class DxuiPopupDismissReason
{
    ClickInsidePopup,
    ClickInsideChainAncestor,
    ClickOutsideChain,
    PointerLeftPopup,
    Manual,
};



class DxuiPopupHost
{
public:
    //
    //  Default opaque background a popup's premultiplied-alpha back
    //  buffer is cleared to when a consumer doesn't override it (the
    //  stock dark menu fill). MUST stay fully opaque (A=0xFF).
    //
    static constexpr uint32_t  kDefaultMenuBackgroundArgb = 0xFF202A35u;

    struct ShowParams
    {
        HWND                            ownerHwnd          = nullptr;
        RECT                            anchorRectScreen   = {};
        DxuiPopupPlacement              placement          = DxuiPopupPlacement::Below;
        bool                            flipIfOffscreen    = true;
        DxuiPopupDismiss                dismiss            = DxuiPopupDismiss::OnClickOutside;
        DxuiPopupInput                  input              = DxuiPopupInput::Interactive;
        // Draw a soft shadow around the popup. The window is enlarged by a
        // margin that holds it, and pointer coordinates are reported relative
        // to the card rather than the window, so a consumer never sees it.
        bool                            shadow             = true;

        // When true (the default) a popup whose dismiss policy is
        // OnClick* grabs the mouse via SetCapture so off-popup clicks
        // route to its WndProc. Consumers that need the OWNER window to
        // keep receiving mouse moves while the popup is up (e.g. a menu
        // bar that hover-switches between top-level titles) set this
        // false: the popup then only sees events when the cursor is
        // directly over it, and the owner drives dismiss/switch.
        bool                            grabsCapture       = true;
        SIZE                            sizeDip            = { 160, 120 };

        // The open animation, in ms; 0 shows the popup outright. Set HERE
        // rather than started after Show, because a reveal begun afterwards
        // has already let one full-size frame reach the screen -- which is
        // the blink it was supposed to replace.
        int                             revealMs           = 0;
        bool                            revealFade         = false;
        std::unique_ptr<DxuiPanel>      content;

        // Background of the card. With a shadow margin the host draws it as
        // a rounded card over a transparent surround; without one it is the
        // clear color and must be fully opaque (A=0xFF), or the owner shows
        // through the premultiplied surface. Defaults to the stock menu
        // background.
        uint32_t                        backgroundArgb     = kDefaultMenuBackgroundArgb;

        // Content render hook. Invoked between the popup painter's
        // Begin/End and the text renderer's BeginDraw/EndDraw, with
        // origin (0,0) at the popup's top-left (popup-local pixels).
        // The consumer draws its menu rows here. Called on Show and on
        // every MarkDirty().
        std::function<void (IDxuiPainter &, IDxuiTextRenderer &)>  renderContent;

        // Pointer-inside callbacks (popup-local pixels). onMoveInside
        // drives hover highlight; onClickInside commits a row. The
        // consumer calls MarkDirty() from these when the visual changes.
        std::function<void (POINT localPx)>                        onMoveInside;
        std::function<void (POINT localPx)>                        onClickInside;

        // Fired when a click OUTSIDE the popup dismisses it, after the
        // dismissal, in SCREEN pixels.
        //
        // The click that closes a popup is spent closing it: the popup holds
        // capture, so the window under the cursor never sees it. That is
        // right for most of a window -- a menu should not fire the button
        // behind it on the way out -- and wrong for a menu bar title, where
        // every other application switches menus on that click. An owner
        // that has somewhere to send it says so here.
        std::function<void (POINT screenPx)>                       onClickOutside;

        // Fired from Close() (manual or auto-dismiss) so the owning
        // widget can clear its own open/active state and return the
        // popup to the host pool. Re-entrant-safe: Close() early-exits
        // when already closed, so calling ReleasePopup() from here does
        // not recurse.
        std::function<void ()>                                     onClosed;
    };


    DxuiPopupHost  ();
    ~DxuiPopupHost ();

    //
    //  Production initialization. The device + context are shared with
    //  (and outlived by) the owning render surface; this object does
    //  NOT AddRef them for ownership purposes — the caller guarantees
    //  they outlive the popup host. The context is needed because the
    //  popup owns its own DxuiPainter (D3D) bound to its swap-chain
    //  back buffer. hInstance is used for window-class registration.
    //
    HRESULT  Initialize         (HINSTANCE              hInstance,
                                 ID3D11Device         * device,
                                 ID3D11DeviceContext  * context);

    //
    //  Test-mode initialization. Skips window-class registration and
    //  HWND creation. All Show()/Close() state machinery still works
    //  but no real OS resources are touched. Tests use this overload
    //  plus the static ComputePlacementForTest seam.
    //
    void     InitializeForTest  ();

    void     Shutdown           ();

    //
    //  Show the popup with the supplied parameters. Computes the
    //  final placement (flipping if requested when the placed rect
    //  would fall outside the monitor work area), promotes the popup
    //  HWND, and renders the content panel into its swap chain.
    //  Returns S_OK on success and stashes the future returned by
    //  GetCompletion(); already-open popups are Close()d first.
    //
    HRESULT  Show      (ShowParams params);

    //
    //  Hide the popup HWND, release its content panel, and resolve
    //  the GetCompletion() future with resultCode. Idempotent.
    //
    void     Close     (int resultCode = 0);

    bool     IsOpen    () const { return m_open; }
    HWND     GetHwnd   () const { return m_hwnd; }

    //
    //  Re-render the popup content NOW (clear to the opaque background,
    //  invoke the render hook, Present). Synchronous + UI-thread-only;
    //  call whenever the popup's visible content changes (hover
    //  highlight, selection, item set, theme, DPI). No-op in test mode
    //  or when the popup is closed / has no render resources.
    //
    void     MarkDirty ();

    //  The open reveal: the menu is rendered once at full size and the WINDOW
    //  then uncovers it, top to bottom, which is the unfold a Windows menu
    //  plays. The swap chain is not resized with the window, so the content
    //  stays put while the frame grows over it and nothing repaints per frame.
    //
    //  A popup placed ABOVE its anchor, because it would not fit below, keeps
    //  its bottom edge pinned and grows upward instead; a menu that unfolded
    //  downward from a flipped position would crawl away from the title that
    //  opened it.
    //
    //  `AdvanceReveal` returns true while more frames are wanted.
    //  Reveal control. Show() starts one itself from `ShowParams`; these are
    //  for a caller driving one on an already-open popup.
    void     BeginReveal   (int durationMs, bool fade);

    //  The closing counterpart: content ramps to transparent and the caller
    //  closes the popup once `AdvanceReveal` reports it is finished. A
    //  tooltip that vanished on the frame its time ran out looked like a
    //  glitch rather than a dismissal.
    void     BeginFadeOut  (int durationMs);
    bool     AdvanceReveal (int64_t nowMs);
    void     ApplyReveal    (float t);
    bool     IsRevealing   () const { return m_revealing; }

    //
    //  Measure the natural extent of `text` (in DIPs) through the
    //  popup's own text renderer. Available any time after Initialize()
    //  -- it only needs the DWrite factory, not a bound back buffer --
    //  so a consumer can self-size its content between AcquirePopup()
    //  and Show(). Returns a failure HRESULT (and 0 extents) in test
    //  mode, where there is no factory; callers fall back to an estimate.
    //
    HRESULT  MeasureText        (const wchar_t  * text,
                                 float            fontSizeDip,
                                 const wchar_t  * fontFamily,
                                 float          & outWidthDip,
                                 float          & outHeightDip);

    //  Word-wrapped variant: extents for the text flowed inside maxWidthDip.
    HRESULT  MeasureTextWrapped (const wchar_t  * text,
                                 float            fontSizeDip,
                                 const wchar_t  * fontFamily,
                                 float            maxWidthDip,
                                 float          & outWidthDip,
                                 float          & outHeightDip);

    //
    //  std::future that resolves when Close() is invoked (or the
    //  popup auto-dismisses). Each Show() resets the promise; only
    //  one outstanding future per Show() cycle is supported.
    //
    std::future<int>  GetCompletion ();

    //
    //  Owner-chain bookkeeping. A child popup (e.g. a cascading
    //  submenu) calls SetParentPopup(parent) so click-outside
    //  classification can walk the chain rather than dismissing the
    //  whole tree when a click lands inside an ancestor popup.
    //
    void              SetParentPopup      (DxuiPopupHost * parent);
    DxuiPopupHost  *  GetParentPopup      () const { return m_parent;       }
    DxuiPopupHost  *  GetActiveChildPopup () const { return m_activeChild;  }

    //
    //  Final rect computed by the most recent Show() (screen coords,
    //  pixels). Exposed for assertions / verification.
    //
    const RECT  &  GetPlacedRectScreenPx () const { return m_placedRectScreenPx; }

    //
    //  Forwarded by DxuiHwndSource's WM_DPICHANGED_BEFOREPARENT
    //  handler so cross-monitor popups re-DPI before the owner does.
    //
    void  HandleDpiChanged  (UINT newDpi);

    const ShowParams  &  GetParams () const { return m_params; }


    //
    //  STATIC TEST SEAMS — pure functions, no HWND, no D3D.
    //

    //
    //  Compute the final popup screen rect given the anchor, the
    //  monitor work area, the preferred placement, and the popup
    //  size. When flipIfOffscreen is true the preferred edge flips
    //  to its opposite (Below -> Above etc.) if placing it on the
    //  preferred edge would push the popup outside the work area.
    //
    static RECT  ComputePlacementForTest (RECT                anchorScreenPx,
                                          RECT                monitorWorkAreaPx,
                                          DxuiPopupPlacement  preferred,
                                          SIZE                popupSizePx,
                                          bool                flipIfOffscreen);

    //
    //  Returns true if a popup configured with `policy` should
    //  dismiss when an event classified as `reason` arrives.
    //
    static bool  ShouldDismissForTest    (DxuiPopupDismiss        policy,
                                          DxuiPopupDismissReason  reason);

private:
    static RECT  GetWorkAreaForRect (const RECT & rectScreenPx);
    static RECT  PlaceOnEdge        (const RECT & anchor, DxuiPopupPlacement edge, SIZE popupSizePx);

    static LRESULT CALLBACK  s_WndProcThunk  (HWND, UINT, WPARAM, LPARAM);
    LRESULT                  WndProc         (UINT msg, WPARAM wp, LPARAM lp);

    HRESULT  EnsureWindowClass               ();
    HRESULT  CreateHwndAndComposition        (const RECT & placedRectScreenPx);
    void     DestroyHwndAndComposition       ();

    //
    //  Back-buffer render-target management. CreateBackBufferRtv binds
    //  the popup swap chain's back buffer as the D3D RTV and the D2D
    //  text target. ResizeSwapChain releases those first (strict order
    //  so ResizeBuffers has no outstanding references), resizes, and
    //  re-binds. RenderNow does the clear / hook / present pass.
    //
    HRESULT  CreateBackBufferRtv             ();
    void     ReleaseBackBufferRtv            ();
    HRESULT  ResizeSwapChain                 (int widthPx, int heightPx);
    void     RenderNow                       ();


    bool                                    m_initialized       = false;
    bool                                    m_testMode          = false;
    HINSTANCE                               m_hInstance         = nullptr;
    ID3D11Device                          * m_device            = nullptr;   // non-owning
    ID3D11DeviceContext                   * m_context           = nullptr;   // non-owning
    HWND                                    m_hwnd              = nullptr;
    bool                                    m_classRegistered   = false;
    std::wstring                            m_className;

    ComPtr<IDXGISwapChain1>                 m_swapChain;
    ComPtr<IDCompositionDevice>             m_compDevice;
    ComPtr<IDCompositionTarget>             m_compTarget;
    ComPtr<IDCompositionVisual>             m_compVisual;
    ComPtr<ID3D11RenderTargetView>          m_rtv;

    // Per-popup render facades bound to the popup's own back buffer. The
    // surface composites with premultiplied alpha (DComp); RenderNow clears
    // it transparent, draws the shadow and the rounded card, and offsets
    // the content hook onto the card.
    DxuiPainter                             m_painter;
    DxuiTextRenderer                        m_textRenderer;
    bool                                    m_renderReady       = false;
    SIZE                                    m_backBufferSizePx  = {};

    //  The client rect less the shadow margin: the card, in window-client
    //  pixels. What the pointer has to be over to count as inside.
    RECT  GetContentClientRect () const;

    //  The drawn shadow and the rounded card it sits under.
    void  PaintShadowAndCard   ();

    ShowParams  m_params;
    bool        m_open               = false;
    int         m_resultCode         = 0;
    RECT        m_placedRectScreenPx = {};
    bool        m_revealing          = false;
    bool        m_revealFade         = false;
    bool        m_revealOut          = false;
    bool        m_revealUpward       = false;
    int         m_revealDurationMs   = 0;
    int64_t     m_revealStartMs      = 0;
    float       m_revealAlpha        = 1.0f;
    RECT        m_windowRectScreenPx = {};
    int         m_shadowMarginPx     = 0;
    UINT        m_dpi                = 0;

    DxuiPopupHost                         * m_parent            = nullptr;
    DxuiPopupHost                         * m_activeChild       = nullptr;

    std::promise<int>                       m_completionPromise;
    bool                                    m_completionPending = false;
};
