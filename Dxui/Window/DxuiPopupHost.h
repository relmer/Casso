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
//  z-order, transparency, and shadow). Shares the parent DxuiHostWindow's
//  ID3D11Device (the device is non-owning).
//
//  Use cases (one popup host per active popup):
//      DxuiDropdown's option list                — placement Below /
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
//      the std::future returned from Completion().
//
//  Test mode (InitializeForTest):
//      No HWND, no device, no swap chain. All placement, dismiss-
//      policy classification, chain bookkeeping, and pool acquire /
//      release behaviour is exercised through pure state mutation
//      and the static ComputePlacementForTest / ShouldDismissForTest
//      seams.
//
//  All public methods are UI-thread-only (FR-083); each entry asserts
//  via DXUI_ASSERT_UI_THREAD().
//
////////////////////////////////////////////////////////////////////////////////


class DxuiHostWindow;
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
        bool                            shadow             = true;
        SIZE                            sizeDip            = { 160, 120 };
        std::unique_ptr<DxuiPanel>      content;

        // Opaque background the popup back buffer is cleared to before
        // the render hook runs. The popup swap chain composites with
        // premultiplied alpha, so this MUST be fully opaque (A=0xFF) or
        // owner content shows through (translucency bug). Defaults to
        // the stock menu background.
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
    //  Completion(); already-open popups are Close()d first.
    //
    HRESULT  Show               (ShowParams params);

    //
    //  Hide the popup HWND, release its content panel, and resolve
    //  the Completion() future with resultCode. Idempotent.
    //
    void     Close              (int resultCode = 0);

    bool     IsOpen             () const { return m_open; }
    HWND     Hwnd               () const { return m_hwnd; }

    //
    //  Re-render the popup content NOW (clear to the opaque background,
    //  invoke the render hook, Present). Synchronous + UI-thread-only;
    //  call whenever the popup's visible content changes (hover
    //  highlight, selection, item set, theme, DPI). No-op in test mode
    //  or when the popup is closed / has no render resources.
    //
    void     MarkDirty          ();

    //
    //  std::future that resolves when Close() is invoked (or the
    //  popup auto-dismisses). Each Show() resets the promise; only
    //  one outstanding future per Show() cycle is supported.
    //
    std::future<int>  Completion ();

    //
    //  Owner-chain bookkeeping. A child popup (e.g. a cascading
    //  submenu) calls SetParentPopup(parent) so click-outside
    //  classification can walk the chain rather than dismissing the
    //  whole tree when a click lands inside an ancestor popup.
    //
    void              SetParentPopup    (DxuiPopupHost * parent);
    DxuiPopupHost  *  ParentPopup       () const { return m_parent;       }
    DxuiPopupHost  *  ActiveChildPopup  () const { return m_activeChild;  }

    //
    //  Final rect computed by the most recent Show() (screen coords,
    //  pixels). Exposed for assertions / verification.
    //
    const RECT  &  PlacedRectScreenPx () const { return m_placedRectScreenPx; }

    //
    //  Forwarded by DxuiHostWindow's WM_DPICHANGED_BEFOREPARENT
    //  handler so cross-monitor popups re-DPI before the owner does.
    //
    void  HandleDpiChanged  (UINT newDpi);

    const ShowParams  &  Params () const { return m_params; }


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

    // Per-popup render facades bound to the popup's own back buffer.
    // The popup composites with premultiplied alpha (DComp), so RenderNow
    // clears the whole buffer opaque before invoking the content hook.
    DxuiPainter                             m_painter;
    DxuiTextRenderer                        m_textRenderer;
    bool                                    m_renderReady       = false;
    SIZE                                    m_backBufferSizePx  = {};

    ShowParams                              m_params;
    bool                                    m_open              = false;
    int                                     m_resultCode        = 0;
    RECT                                    m_placedRectScreenPx = {};

    DxuiPopupHost                         * m_parent            = nullptr;
    DxuiPopupHost                         * m_activeChild       = nullptr;

    std::promise<int>                       m_completionPromise;
    bool                                    m_completionPending = false;
};
