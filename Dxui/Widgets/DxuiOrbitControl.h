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
//  AN ARROW TURNS ON THE WAY DOWN, and keeps turning while it is held. Not
//  on release, which is how a button commits and how this control used to
//  behave: a button commits on release so a press can be taken back, and
//  nothing here needs taking back -- a step in the wrong direction is undone
//  by the arrow facing it. Waiting for the release only made the scene lag
//  the finger.
//
//  HOLDING IT REPEATS, unless the pointer moves. Travel past the slop turns
//  the press into a free drag along that arrow's axis, and the repeat does
//  not come back for the rest of that press however still the hand goes
//  afterward -- a drag that paused is still a drag, and firing steps into
//  the middle of one would fight the hand that is aiming.
//
//  The central orb is home, and it alone still commits on RELEASE: it throws
//  away the framing the user built, so dragging off it has to remain the way
//  to change your mind.
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

    // A step: the arrow it was on. dxPx/dyPx on the drag callback are the
    // travel since the LAST report, not since the press.
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

    // Drives the held-arrow repeat. The caller owns the clock, as it does
    // for the chrome tooltips: a widget that reads the time itself cannot be
    // tested without waiting for it.
    void  Tick (int64_t nowMs) override;

    // Whether a repeat is pending, so the host can tick fast enough to serve
    // it instead of parking until the next frame or message. Mirrors the
    // tooltips' WantsTick.
    bool  WantsTick () const { return m_armed && !m_repeatBlocked && m_armed_on != Part::Orb; }

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

    // Held-arrow repeat, in milliseconds: the pause before it starts, and
    // the interval after. The pause is what keeps a single deliberate click
    // from turning into two.
    static constexpr int64_t  kRepeatDelayMs    = 400;
    static constexpr int64_t  kRepeatIntervalMs = 110;

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

    // The repeat's own state. `m_repeatBlocked` latches for the rest of the
    // press once the pointer has moved, and only the release clears it.
    // `m_repeatAtMs` is 0 until the first tick schedules it, which is what
    // keeps the clock out of the pointer handlers.
    bool     m_repeatBlocked = false;
    int64_t  m_repeatAtMs    = 0;
};
