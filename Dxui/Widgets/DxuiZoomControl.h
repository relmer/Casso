#pragma once

#include "Pch.h"

#include "Core/IDxuiControl.h"
#include "Widgets/DxuiZoomPopup.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiZoomControl
//
//  A DxuiZoomPopup as a control: at rest the "Zoom 100%" button in the
//  bottom-right corner of its bounds, and with the pointer over it a panel
//  above with the slider, Reset and the percentage. The panel may grow
//  past the control's bounds, up to the top of its panel bounds. The
//  window has the zoom: a change made on the control goes to the
//  callback, and SetZoom sets the zoom the control draws.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiZoomControl : public IDxuiControl
{
public:
    using ZoomFn = DxuiZoomPopup::ZoomFn;
    using StepFn = DxuiZoomPopup::StepFn;

    //  The button's width, the room the control takes at rest.
    static constexpr int     kWidthDip  = DxuiZoomPopup::kButtonWidthDip;
    static constexpr double  kMinZoom   = 0.5;
    static constexpr double  kMaxZoom   = 2.0;

    DxuiZoomControl();

    DxuiZoomControl             (const DxuiZoomControl &) = delete;
    DxuiZoomControl & operator= (const DxuiZoomControl &) = delete;

    void    SetZoom        (double zoom)        { m_popup.SetZoom (zoom); }
    double  GetZoom        () const             { return m_popup.GetZoom(); }
    void    SetOnZoom      (ZoomFn fn)          { m_popup.SetOnZoom (std::move (fn)); }
    void    SetOnStep      (StepFn fn)          { m_popup.SetOnStep (std::move (fn)); }
    void    SetResetZoom   (double zoom)        { m_popup.SetResetZoom (zoom); }
    void    SetShowBox     (bool show)          { m_popup.SetShowBox (show); }
    void    SetRange       (double minZoom, double maxZoom, bool isLogarithmic);
    void    SetPanelBounds (const RECT & bounds) { m_panelBounds = bounds; }

    bool    IsSliderShown  () const             { return m_popup.IsOpen(); }
    bool    IsDragging     () const             { return m_popup.IsDragging(); }
    bool    IsOverControls (POINT p) const      { return m_popup.HitTest (p) != DxuiZoomPopup::Hit::None; }
    RECT    GetButtonRect  () const             { return m_popup.GetButtonRect(); }

    const DxuiZoomPopup & GetPopup () const     { return m_popup; }

    //  The panel opens with the cursor over the button and closes once the
    //  cursor moves away, and the opening is moved along to nowMs. True while
    //  the panel changes, so the host draws again.
    bool    UpdateHover    (POINT cursorPx, int64_t nowMs);

    //  The host's zoom, unless the thumb is being dragged, and the hover by
    //  where the cursor is in `owner`'s client area, so the panel stays open
    //  through a tooltip that ends the window's hover. True as UpdateHover.
    bool    Follow         (HWND owner, double zoom, int64_t nowMs);

    //  The button at the right end of a row, centered on it, and the panel
    //  no higher than the top of panelBoundsPx.
    void    PlaceInRow     (const RECT & rowPx, const RECT & panelBoundsPx, const DxuiDpiScaler & scaler);

    //  The zoom a number of steps from `zoom` through the standard stops, as
    //  a browser zooms: -1 for the next stop down, 1 for the next one up.
    static double  StepZoom (double zoom, int steps);

    void  Layout  (const RECT & boundsPx, const DxuiDpiScaler & scaler)                  override;
    void  Paint   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse (const DxuiMouseEvent & ev)                                          override;
    void  Tick    (int64_t nowMs)                                                      override;

    //  Whether any of the panel is drawn; the host ticks while it is.
    bool  IsPanelShown () const { return m_popup.IsShown(); }

private:
    bool  Advance   (int64_t nowMs, bool wasOpen);
    void  FitBounds ();

    DxuiZoomPopup  m_popup;
    RECT           m_panelBounds = {};
};
