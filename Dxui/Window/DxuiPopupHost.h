#pragma once

#include "Pch.h"
#include "Core/DxuiPanel.h"



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
    };


    DxuiPopupHost  ();
    ~DxuiPopupHost ();

    //
    //  Production initialization. The device is shared with (and
    //  outlived by) the owning DxuiHostWindow; this object does NOT
    //  AddRef it for ownership purposes — the caller guarantees
    //  the device outlives the popup host. hInstance is used for
    //  window-class registration.
    //
    HRESULT  Initialize         (HINSTANCE hInstance, ID3D11Device * device);

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


    bool                                    m_initialized       = false;
    bool                                    m_testMode          = false;
    HINSTANCE                               m_hInstance         = nullptr;
    ID3D11Device                          * m_device            = nullptr;   // non-owning
    HWND                                    m_hwnd              = nullptr;
    bool                                    m_classRegistered   = false;
    std::wstring                            m_className;

    ComPtr<IDXGISwapChain1>                 m_swapChain;
    ComPtr<IDCompositionDevice>             m_compDevice;
    ComPtr<IDCompositionTarget>             m_compTarget;
    ComPtr<IDCompositionVisual>             m_compVisual;

    ShowParams                              m_params;
    bool                                    m_open              = false;
    int                                     m_resultCode        = 0;
    RECT                                    m_placedRectScreenPx = {};

    DxuiPopupHost                         * m_parent            = nullptr;
    DxuiPopupHost                         * m_activeChild       = nullptr;

    std::promise<int>                       m_completionPromise;
    bool                                    m_completionPending = false;
};
