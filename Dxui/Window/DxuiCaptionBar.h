#pragma once

#include "Pch.h"
#include "Core/DxuiPanel.h"
#include "DxuiSystemButton.h"



////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCaptionBar
//
//  Container panel covering the custom-chrome title-bar strip at the
//  top of a `DxuiHwndSource`. Inherits all `DxuiPanel` semantics
//  (children, layout, paint fan-out) and adds caption-specific hit-
//  test defaults: any point not consumed by a child resolves to
//  `DxuiHitTestKind::Caption`, giving Win32 free drag / double-click-
//  to-maximize on blank areas of the title bar.
//
//  Two usage modes:
//
//    1. Plain container (DxuiDialog): the caller Adopts its own title
//       label + Close DxuiSystemButton and positions them via a layout.
//       The bar paints nothing of its own (flat panel).
//
//    2. Host-owned caption (DxuiHwndSource, the SetWindowText model):
//       ConfigureButtons() builds the owned min/max/close
//       DxuiSystemButton children and turns on self-rendering of the
//       gradient + app icon + title text. The consuming app never
//       touches this object -- it just sets the window title / icon on
//       the host, which forwards here. This is what replaces a bespoke
//       per-window title bar.
//
//  All public methods are called on the UI thread (FR-083).
//
////////////////////////////////////////////////////////////////////////////////



class DxuiCaptionBar : public DxuiPanel
{
public:
    enum class Buttons
    {
        None,
        CloseOnly,
        MinMaxClose,
    };

    DxuiCaptionBar  ();
    ~DxuiCaptionBar () override = default;

    // Caption-strip height. The DIP value is the design constant; the
    // pixel helpers scale it. HeightPxForDpi lets a consumer reserve the
    // band before a host (and thus a live scaler) exists.
    static constexpr int  kCaptionHeightDip = 32;
    static int            HeightPxForDpi    (UINT dpi);

    // --- Host-owned caption configuration (mode 2) ---------------------

    void  ConfigureButtons (Buttons buttons);
    void  SetSystemHwnd    (HWND hwnd);
    void  SetTitle         (const std::wstring & title);
    void  SetAppIcon       (std::vector<uint32_t> bgraPremul, int widthPx, int heightPx);
    void  SetMaximized     (bool maximized);

    // Natural caption-strip height in physical pixels for the given DPI;
    // the host reserves this band at the top and lays content out below.
    int   PreferredHeightPx  (const DxuiDpiScaler & scaler) const;
    int   PreferredHeightDip () const;

    // --- IDxuiControl / DxuiPanel overrides ----------------------------

    void  Layout (const RECT          & boundsDip,
                  const DxuiDpiScaler & scaler) override;
    void  Paint  (IDxuiPainter        & painter,
                  IDxuiTextRenderer   & text,
                  const IDxuiTheme    & theme) override;

    DxuiHitTestKind     ClassifyHit    (POINT clientDip) const override;
    DxuiAccessibleRole  AccessibleRole () const          override { return DxuiAccessibleRole::CaptionBar; }

private:
    bool                               m_renderCaption = false;
    Buttons                            m_buttons       = Buttons::None;
    std::wstring                       m_title;
    std::vector<uint32_t>              m_iconPixels;
    int                                m_iconW         = 0;
    int                                m_iconH         = 0;
    bool                               m_maximized     = false;
    DxuiDpiScaler                      m_scaler;
    std::unique_ptr<DxuiSystemButton>  m_minBtn;
    std::unique_ptr<DxuiSystemButton>  m_maxBtn;
    std::unique_ptr<DxuiSystemButton>  m_closeBtn;
};
