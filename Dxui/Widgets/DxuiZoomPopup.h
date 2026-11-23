#pragma once

#include "Pch.h"

#include "Core/DxuiDpiScaler.h"

class  IDxuiTextRenderer;
class  IDxuiTheme;
struct DxuiMouseEvent;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiZoomPopup
//
//  A zoom in a view's bottom-right corner: a button that has the zoom as a
//  percentage, and, opened by hovering over it, a panel that grows out of
//  it: the slider, across the zoom range, along the top row, and on the
//  button's row Reset at the left and the percentage at the right, where the
//  button has it. A logarithmic range runs the slider on the zoom's
//  logarithm, so each step along it scales the view by the same factor. The
//  panel grows out of the button as it opens and shrinks back into it as it
//  closes, over kAnimateMs, its contents revealed by the growing edge rather
//  than moved.
//
//  It is drawn with the text renderer, over the view's picture. The host
//  places it against the view's rect, sets the zoom to draw, and either acts
//  on what a press lands on or passes its mouse events to OnMouse, which
//  calls the zoom and step callbacks.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiZoomPopup
{
public:
    //  What a point is over.
    enum class Hit
    {
        None,
        Button,
        Panel,
        Track,
        Reset,
    };

    static constexpr int  kButtonWidthDip  = 84;
    static constexpr int  kButtonHeightDip = 20;
    static constexpr int  kPanelWidthDip   = 220;
    static constexpr int  kMarginDip       = 6;
    static constexpr int  kPadDip          = 10;
    static constexpr int  kRowDip          = 20;
    static constexpr int  kResetWidthDip   = 56;
    static constexpr int  kThumbDip        = 6;
    static constexpr int  kPercent         = 100;

    //  How long the panel takes to open or close.
    static constexpr int64_t  kAnimateMs   = 140;

    //  How far past the button and its open panel the pointer can go before
    //  the panel closes, as the disk inspector's tilt slider does.
    static constexpr int  kNearDip         = 16;

    //  Above this percentage the label is a factor, "Zoom 4096x", so it fits
    //  the button.
    static constexpr int  kMostPercent     = 9999;

    using ZoomFn = std::function<void (double zoom)>;
    using StepFn = std::function<double (double zoom, int steps)>;

    //  The zoom as a percentage, "Zoom 300%", so the button reads as a zoom
    //  and not as a figure about the view.
    static int           GetPercent      (double zoom);
    static std::wstring  GetPercentLabel (double zoom);

    //  The zoom range the slider runs over, the zoom Reset goes to, and the
    //  callbacks OnMouse calls: a new zoom, and a wheel notch's step.
    void    SetRange     (double minZoom, double maxZoom, bool isLogarithmic);
    void    SetResetZoom (double zoom)  { m_resetZoom = zoom; }
    void    SetOnZoom    (ZoomFn fn)    { m_onZoom = std::move (fn); }
    void    SetOnStep    (StepFn fn)    { m_onStep = std::move (fn); }
    void    SetZoom      (double zoom)  { m_zoom = std::clamp (zoom, m_minZoom, m_maxZoom); }
    double  GetZoom      () const       { return m_zoom; }

    //  How far along the slider, from 0 to 1, a zoom sits in the range, and
    //  the zoom at a place along it.
    float   GetPosition       (double zoom) const;
    double  GetZoomAtPosition (float position) const;

    //  Placed in the bottom-right corner of corner, its panel inside
    //  the view's bounds.
    void  Place (const RECT & corner, const RECT & bounds, const DxuiDpiScaler & scaler);

    //  Another control's rect as the button, which the widget doesn't draw
    //  while closed; open, the panel grows out of it and draws the label as
    //  that control does: left-aligned labelInsetDip in, in the body face at
    //  labelFontDip, centered on its cap height.
    void  PlaceAtButton (const RECT & button, const RECT & bounds, const DxuiDpiScaler & scaler, float labelInsetDip, float labelFontDip);

    RECT  GetButtonRect () const { return m_button; }
    RECT  GetPanelRect  () const;
    RECT  GetTrackRect  () const;
    RECT  GetResetRect  () const;

    bool  IsOpen      () const { return m_isOpen; }
    void  SetOpen     (bool open);

    //  Whether any of the panel is drawn: it is open, or its closing
    //  animation is running.
    bool  IsShown     () const { return m_isOpen || m_openness > 0.0f; }

    //  Moves the opening or closing along; true while it goes on, so the
    //  host draws again. The panel as drawn at this point, the button's
    //  rect grown toward GetPanelRect by how far open it is.
    bool  Tick              (int64_t nowMs);
    RECT  GetShownPanelRect () const;
    bool  IsDragging        () const { return m_isDragging; }

    //  The pointer over the button opens the panel, unless a press on the
    //  button closed it; then not until the pointer moves off the button.
    void  OnHover     (POINT point);
    void  Toggle      ();
    void  SetDragging (bool dragging) { m_isDragging = dragging && m_isOpen; }

    Hit   HitTest     (POINT point) const;
    bool  IsNear      (POINT point) const;

    //  The zoom at the point along the track, the ends taken past
    //  either end.
    double  GetZoomAt     (POINT point) const;
    float   GetPositionAt (POINT point) const;

    //  A mouse event in the host's coordinates: hover, a press on the
    //  button, the track or Reset, a drag of the thumb, and the wheel over
    //  the button or the panel. True when the event was on the popup.
    bool  OnMouse    (const DxuiMouseEvent & ev);

    void  Paint      (IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    void  PaintPanel (IDxuiTextRenderer & text, const IDxuiTheme & theme, const std::wstring & label, float position) const;

private:
    static bool  Contains         (const RECT & rect, POINT point);
    RECT         GetFullPanelRect () const;
    RECT         GetResetRectIn   (const RECT & panel) const;
    RECT         GetTrackRectIn   (const RECT & panel) const;
    static void  PaintBox         (IDxuiTextRenderer & text, const RECT & rect, uint32_t fill, uint32_t edge);
    void         PaintLabel       (IDxuiTextRenderer & text, const IDxuiTheme & theme, const std::wstring & label) const;
    void         ChangeZoom       (double zoom);

    DxuiDpiScaler  m_scaler;
    RECT           m_button        = {};
    RECT           m_bounds        = {};
    bool           m_isOpen        = false;
    bool           m_isDragging    = false;
    bool           m_isOwnButton   = true;
    bool           m_isHoverHeld   = false;   // a press closed the panel with the pointer on the button
    float          m_labelInset    = 0.0f;    // another control's label: its inset, in DIPs
    float          m_labelFont     = 0.0f;    // and its font size, in DIPs
    float          m_openness      = 0.0f;    // 0 closed to 1 open, as drawn
    float          m_animFrom      = 0.0f;    // the openness the animation started from
    int64_t        m_animStartMs   = -1;      // when it started, -1 for none
    bool           m_isAnimPending = false;   // started by SetOpen, timed from the next Tick
    double         m_zoom          = 1.0;
    double         m_minZoom       = 1.0;
    double         m_maxZoom       = 1.0;
    double         m_resetZoom     = 1.0;
    bool           m_isLogarithmic = true;
    ZoomFn         m_onZoom;
    StepFn         m_onStep;
};
