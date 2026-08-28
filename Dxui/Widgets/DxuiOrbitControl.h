#pragma once

#include "Pch.h"

#include "Core/IDxuiControl.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiOrbitControl
//
//  The scene compass: four arrows around a central orb, laid over the 3D
//  scene as the VISIBLE way to turn it. The drag gestures still work; this
//  is the affordance that tells a user who has never tried dragging that
//  the scene turns at all.
//
//  A CLICK on an arrow turns the scene a fixed step in that direction; a
//  PRESS-AND-DRAG from an arrow turns it freely along that arrow's axis
//  until release. Which gesture happened is decided the way buttons decide
//  it everywhere: travel past a small threshold makes it a drag, release
//  inside the threshold makes it a click. The central orb is home -- one
//  click squares the scene back up.
//
//  Like the HUD notice, this floats over live content and cannot assume a
//  background, so it paints its own soft dark backdrop and keeps every
//  mark translucent until the pointer is over it -- present when looked
//  for, quiet when not.
//
//  The control is deliberately DUMB about what turning means: it reports
//  parts, steps and pixel travel through callbacks, and the shell owns the
//  mapping to yaw and pitch. The one mapping choice made here is none --
//  even the axis lock is the caller's, since the part is in the callback.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiOrbitControl : public IDxuiControl
{
public:
    enum class Part
    {
        None,
        Up,
        Down,
        Left,
        Right,
        Orb,
    };

    // A click's fixed step: the arrow it was on. dxPx/dyPx on the drag
    // callback are the travel since the LAST report, not since the press.
    using StepFn = std::function<void (Part part)>;
    using DragFn = std::function<void (Part part, float dxPx, float dyPx)>;
    using HomeFn = std::function<void ()>;

    DxuiOrbitControl  () = default;
    ~DxuiOrbitControl () override = default;

    void  SetOnStep (StepFn fn) { m_onStep = std::move (fn); }
    void  SetOnDrag (DragFn fn) { m_onDrag = std::move (fn); }
    void  SetOnHome (HomeFn fn) { m_onHome = std::move (fn); }

    void  SetRect (const RECT & rectPx) { SetBounds (rectPx); }
    void  SetDpi  (UINT dpi)            { m_dpi = dpi; }

    // Where a point lands on the compass. Exposed because the shell routes
    // raw mouse itself (the way the toolbar and the drive chrome are
    // routed) rather than through a panel-tree dispatch.
    Part  HitPart (int xPx, int yPx) const;

    // The shell's forwarding surface. Down returns whether the press was
    // taken (and so must also see the matching move/up); move and up return
    // whether they consumed the event. Up fires the click or ends the drag.
    bool  OnPointerDown (int xPx, int yPx);
    bool  OnPointerMove (int xPx, int yPx);
    bool  OnPointerUp   (int xPx, int yPx);

    bool  Dragging () const { return m_armed && m_dragging; }

    //
    //  IDxuiControl.
    //
    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint  (IDxuiPainter      & painter,
                  IDxuiTextRenderer & text,
                  const IDxuiTheme  & theme) override;

    std::wstring        AccessibleName () const override { return L"Rotate scene"; }
    DxuiAccessibleRole  AccessibleRole () const override { return DxuiAccessibleRole::Generic; }

    // How far a press may travel and still be a click. Generous, because
    // the compass sits over a scene where a stray pixel of motion is
    // nothing, and a click misread as a hair-width drag does nothing at
    // all -- the least forgivable outcome for a discoverability control.
    static constexpr int  kClickSlopPx = 4;

private:
    // The geometry is derived from the bounds every time it is needed --
    // center, orb radius, arrow extent -- so there is no cached layout to
    // fall out of date when the shell moves the compass with the viewport.
    void  Metrics (float & cx, float & cy, float & orbR, float & reach) const;

    StepFn  m_onStep;
    DragFn  m_onDrag;
    HomeFn  m_onHome;

    UINT    m_dpi       = 96;
    Part    m_hover     = Part::None;
    Part    m_armed_on  = Part::None;
    bool    m_armed     = false;
    bool    m_dragging  = false;
    POINT   m_pressPx   = {};
    POINT   m_lastPx    = {};
};
