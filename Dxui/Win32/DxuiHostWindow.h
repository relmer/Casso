#pragma once

#include "Pch.h"
#include "Core/DxuiPanel.h"
#include "Core/DxuiFocusManager.h"
#include "Core/DxuiDpiScaler.h"
#include "Render/DxuiPainter.h"
#include "Render/DxuiTextRenderer.h"



////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHostWindow
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
//      Adopt mode (existing HWND, host does not call CreateWindow /
//          DestroyWindow / does not own the swap chain) lands in
//          Phase 8 alongside the main-window NC delegation. Not
//          implemented in this phase.
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



enum class DxuiHostWindowBackdrop
{
    None,
    Mica,
};



class DxuiHostWindow
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
        DxuiHostWindowBackdrop   backdrop         = DxuiHostWindowBackdrop::Mica;
        float                    resizeBorderDip  = 6.0f;
        SIZE                     initialSizeDip   = { 1024, 768 };
    };


    DxuiHostWindow  ();
    ~DxuiHostWindow ();

    HRESULT  Create            (const CreateParams & params);
    void     Destroy           ();

    HWND          Hwnd          () const { return m_hwnd; }
    DxuiPanel  &  Root          ()       { return *m_root; }
    void          SetTheme      (const IDxuiTheme * theme);

    LRESULT  WndProc           (UINT msg, WPARAM wp, LPARAM lp);

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
        DxuiHostWindowBackdrop  backdropRequested     = DxuiHostWindowBackdrop::None;
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
    DxuiHostWindow (RECT                         clientBoundsDip,
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
    void     ApplyDwmConfiguration     ();

    LRESULT  HandleNcCalcSize          (WPARAM wp, LPARAM lp);
    LRESULT  HandleNcHitTest           (LPARAM lp);
    LRESULT  HandleNcMouse             (UINT msg, WPARAM wp, LPARAM lp);
    void     HandleDpiChanged          (WPARAM wp, LPARAM lp);
    void     HandleSize                (LPARAM lp);
    void     HandleThemeChange         ();

    DxuiHitTestKind  ClassifyHitInternal (POINT clientDip, RECT clientBoundsDip) const;

    static DxuiHitTestKind  ClassifyResizeEdge (POINT clientDip,
                                                RECT  clientBoundsDip,
                                                int   resizeBorderPx);


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
    DxuiFocusManager                  m_focusManager;
    const IDxuiTheme *                m_theme              = nullptr;

    bool                              m_ownsHwnd           = false;
    bool                              m_synthetic          = false;
    bool                              m_classRegistered    = false;

#ifdef _DEBUG
    DwmAppliedSeam                    m_dwmSeam;
#endif
};
