#include "Pch.h"

#include "Core/DxuiPanZoom.h"

#include "Core/DxuiInput.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPanZoom::DxuiPanZoom
//
////////////////////////////////////////////////////////////////////////////////

DxuiPanZoom::DxuiPanZoom (const Config & cfg)
    : m_cfg (cfg)
{
    m_zoom.cur    = cfg.zoomMin;
    m_zoom.target = cfg.zoomMin;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPanZoom::OnWheel
//
//  Three gestures share the wheel: Ctrl-wheel zooms, a horizontal wheel
//  (or two-finger sideways pan) slides X, and a plain wheel scrolls Y.
//  Whichever the config disables falls through unhandled.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPanZoom::OnWheel (const DxuiMouseEvent & ev)
{
    bool  handled = false;



    if (ev.wheelDelta == 0.0f)
    {
        // A zero-delta notification carries no motion to apply.
    }
    else if (ev.ctrl && !ev.wheelHorizontal && m_cfg.enableZoom)
    {
        // Ctrl + wheel zooms (this is also how a Precision Touchpad pinch
        // arrives). Vertical wheel only -- a horizontal pinch is nonsense.
        // Anchored on the cursor so the content under it stays fixed.
        ApplyZoomFactor (ev.wheelDelta > 0.0f ? m_cfg.zoomStep : 1.0 / m_cfg.zoomStep,
                         /*anchored*/ true,
                         (float) ev.positionDip.x, (float) ev.positionDip.y);
        handled = true;
    }
    else if (ev.wheelHorizontal && m_cfg.enablePanX)
    {
        NudgePanX ((double) ev.wheelDelta * m_cfg.wheelPanX);
        handled = true;
    }
    else if (!ev.wheelHorizontal && m_cfg.enablePanY)
    {
        // Wheel up (+delta) reveals earlier content -> pan target DECREASES;
        // the caller decides sign meaning via bounds, we keep +wheel = -pan
        // so "scroll up = go back" matches every other scroll surface.
        NudgePanY (-(double) ev.wheelDelta * m_cfg.wheelPanY, /*user*/ true);
        handled = true;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPanZoom::OnMouse
//
//  Wheel gestures live in OnWheel; the drag states are small enough to sit
//  inline. Anything unclaimed falls through to the caller.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPanZoom::OnMouse (const DxuiMouseEvent & ev)
{
    bool    isLeft  = (ev.button == DxuiMouseButton::Left);
    bool    handled = false;
    double  dx      = 0.0;
    double  dy      = 0.0;



    switch (ev.kind)
    {
    case DxuiMouseEventKind::Wheel:
        handled = OnWheel (ev);
        break;

    case DxuiMouseEventKind::Down:
        if (isLeft && m_cfg.enableDrag)
        {
            m_dragging = true;
            m_dragLast = ev.positionDip;
            handled    = true;
        }

        break;

    case DxuiMouseEventKind::Move:
        if (m_dragging)
        {
            dx         = (double) (ev.positionDip.x - m_dragLast.x);
            dy         = (double) (ev.positionDip.y - m_dragLast.y);
            m_dragLast = ev.positionDip;

            // A drag FRAMES the magnified view (moving the camera), it does not
            // scroll the document -- that stays on the wheel / arrows. Content
            // follows the finger: drag right reveals the left, drag down reveals
            // what is above. panX is world-X, panYCam is the inverted screen->
            // world Y, so both track the cursor.
            if (m_cfg.enablePanX && dx != 0.0)
            {
                NudgePanX (-dx * (double) m_dragPerPxX);
            }

            if (dy != 0.0)
            {
                NudgePanYCam (dy * (double) m_dragPerPxY);
            }

            handled = true;
        }

        break;

    case DxuiMouseEventKind::Up:
        if (m_dragging && isLeft)
        {
            m_dragging = false;
            handled    = true;
        }

        break;

    default:
        break;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPanZoom::OnKey
//
//  The zoom chords: Ctrl+Plus, Ctrl+Minus, Ctrl+0.
//
//  Both spellings of each key are accepted -- the main row and the numeric
//  keypad -- because Windows reports them as different virtual keys and a user
//  who zooms from the keypad has no reason to know that.
//
//  Only Ctrl+key-down is claimed. Plain Plus and Minus belong to whatever else
//  has focus, and claiming them would break typing in any field the pan-zoom
//  surface happens to contain.
//
//  Zoom steps are MULTIPLICATIVE, and Ctrl+Minus applies the reciprocal of the
//  same factor. That makes zoom-in followed by zoom-out land exactly back
//  where it started, which an additive step could not.
//
//  Everything routes through ApplyZoomFactor and ResetZoom rather than
//  assigning the zoom directly, so the anchor and clamp rules apply to the
//  keyboard exactly as they do to the wheel.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPanZoom::OnKey (const DxuiKeyEvent & ev)
{
    // Only Ctrl+key-down is ours, and only when zoom is enabled at all.
    bool  zooms   = (ev.kind == DxuiKeyEventKind::Down) && ev.ctrl && m_cfg.enableZoom;
    bool  handled = false;



    if (zooms)
    {
        handled = true;   // the default arm below takes it back

        switch (ev.vk)
        {
        case VK_OEM_PLUS:
        case VK_ADD:
            ApplyZoomFactor (m_cfg.zoomStep);
            break;

        case VK_OEM_MINUS:
        case VK_SUBTRACT:
            ApplyZoomFactor (1.0 / m_cfg.zoomStep);
            break;

        case '0':
        case VK_NUMPAD0:
            ResetZoom();
            break;

        default:
            handled = false;
            break;
        }
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPanZoom::Tick
//
//  Advances every animated axis toward its target and reports whether anything
//  is still in flight -- which is what tells the caller to keep asking for
//  frames.
//
//  Time is passed IN rather than read here, so the animation is driven by the
//  caller's frame clock and is fully deterministic under test.
//
//  A zero or negative delta -- the very first tick, or two ticks landing in
//  the same instant -- advances nothing but still reports honestly whether a
//  glide is outstanding. Returning false there would end the animation on its
//  first frame and leave every axis stranded mid-glide.
//
//  Zoom eases on its own time constant, separate from panning, because a zoom
//  that settles at the pan rate reads as sluggish while a pan at the zoom rate
//  reads as twitchy.
//
//  NotifyChanged() fires once per tick rather than once per axis, so a frame that
//  moves all five axes still produces a single notification.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPanZoom::Tick (double nowSec)
{
    double  dt     = (m_lastTickSec < 0.0) ? 0.0 : (nowSec - m_lastTickSec);
    bool    moving = false;



    m_lastTickSec = nowSec;

    if (dt <= 0.0)
    {
        // First tick, or two ticks in the same instant: nothing to advance,
        // but report whether a glide is still in flight so the caller keeps
        // asking for frames.
        moving = (m_zoom.cur != m_zoom.target) || (m_panX.cur != m_panX.target)
              || (m_panY.cur != m_panY.target) || (m_panYCam.cur != m_panYCam.target)
              || (m_overscrollY.cur != m_overscrollY.target);
    }
    else
    {
        moving |= EaseToward (m_zoom, dt, m_cfg.zoomEaseTauSec);
        moving |= EaseToward (m_panX, dt, m_cfg.easeTauSec);
        moving |= EaseToward (m_panY, dt, m_cfg.easeTauSec);
        moving |= EaseToward (m_panYCam, dt, m_cfg.easeTauSec);
        moving |= EaseToward (m_overscrollY, dt, m_cfg.easeTauSec);

        if (moving)
        {
            NotifyChanged();
        }
    }

    return moving;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPanZoom::SetPanYBounds
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanZoom::SetPanYBounds (float lo, float hi)
{
    m_panYlo = lo;
    m_panYhi = hi;
    ClampTargets();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPanZoom::SetPanXBounds
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanZoom::SetPanXBounds (float lo, float hi)
{
    m_panXlo = lo;
    m_panXhi = hi;
    ClampTargets();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPanZoom::SetPanYCamBounds
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanZoom::SetPanYCamBounds (float lo, float hi)
{
    m_panYCamLo = lo;
    m_panYCamHi = hi;
    ClampTargets();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPanZoom::SetDragScale
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanZoom::SetDragScale (float contentPerPixelX, float contentPerPixelY)
{
    m_dragPerPxX = contentPerPixelX;
    m_dragPerPxY = contentPerPixelY;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPanZoom::SetPanYTarget
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanZoom::SetPanYTarget (float y)
{
    double  clamped = y;
    bool    changed = false;



    if (m_panYhi >= m_panYlo)
    {
        clamped = std::min (std::max ((double) y, m_panYlo), m_panYhi);
    }

    // Follow mode owns the paper position again, so spring any world overscroll
    // back home (eases via Tick), returning the view to its resting frame.
    changed = (clamped != m_panY.target) || (m_overscrollY.target != 0.0);

    m_panY.target        = clamped;
    m_overscrollY.target = 0.0;

    if (changed)
    {
        NotifyChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPanZoom::PanByUser
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanZoom::PanByUser (float deltaContentX, float deltaContentY)
{
    if (m_cfg.enablePanX && deltaContentX != 0.0f)
    {
        NudgePanX ((double) deltaContentX);
    }

    if (m_cfg.enablePanY && deltaContentY != 0.0f)
    {
        NudgePanY ((double) deltaContentY, /*user*/ true);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPanZoom::SnapPanY
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanZoom::SnapPanY (float y)
{
    m_panY.cur    = y;
    m_panY.target = y;
    m_overscrollY.cur    = 0.0;   // torn / replaced content: world back to home
    m_overscrollY.target = 0.0;
    ClampTargets();
    NotifyChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPanZoom::ZoomIn
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanZoom::ZoomIn()
{
    ApplyZoomFactor (m_cfg.zoomStep);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPanZoom::ZoomOut
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanZoom::ZoomOut()
{
    ApplyZoomFactor (1.0 / m_cfg.zoomStep);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPanZoom::ResetZoom
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanZoom::ResetZoom()
{
    if (m_zoom.target != (double) m_cfg.zoomMin)
    {
        m_zoom.target = m_cfg.zoomMin;
        NotifyChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPanZoom::ApplyZoomFactor
//
//  Multiplies the zoom target by a factor, optionally keeping the content
//  under a cursor position fixed.
//
//  Cursor-anchored zoom is the whole substance here. The visible content span
//  scales by z0/z1, so a point (anchor - center) pixels off-center moves by
//  that same fraction; countering it needs a pan of
//
//    (anchor - center) * contentPerPixel * (1 - z0/z1)
//
//  with contentPerPixel taken at the PRE-zoom magnification, since that is the
//  scale the displacement was measured in.
//
//  The vertical sign is opposite the horizontal one because screen Y increases
//  downward while the camera's world Y increases upward -- not an error, and
//  the thing to check first if anchored zoom ever drifts the wrong way.
//
//  Buttons and keyboard chords pass anchored = false and zoom about the center,
//  which is what makes Ctrl+Plus feel stable while wheel zoom feels like it
//  follows the pointer.
//
//  The clamp is applied BEFORE the anchor math and an unchanged zoom returns
//  early, so zooming into the limit does not creep the pan on every further
//  notch.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanZoom::ApplyZoomFactor (double factor, bool anchored, float anchorX, float anchorY)
{
    double z0 = m_zoom.target;
    double z1 = std::min (std::max (z0 * factor, (double) m_cfg.zoomMin), (double) m_cfg.zoomMax);



    if (z1 == z0)
    {
        return;
    }

    m_zoom.target = z1;

    // Cursor-anchored zoom: shift the pan targets so the content point under the
    // cursor stays put. The visible content span scales by z0/z1, so a point
    // (anchor - center) pixels off-center moves by that fraction; countering it
    // needs delta_content = (anchor - center) * contentPerPixel * (1 - z0/z1),
    // where contentPerPixel is the drag scale at the pre-zoom magnification.
    // Buttons / keys pass anchored = false and zoom about the center untouched.
    if (anchored)
    {
        double  s = 1.0 - z0 / z1;

        if (m_cfg.enablePanX && m_dragPerPxX != 0.0f)
        {
            NudgePanX (((double) anchorX - (double) m_viewCenterX) * (double) m_dragPerPxX * s);
        }

        if (m_dragPerPxY != 0.0f)
        {
            // Frame vertically toward the cursor. Screen Y is inverted from the
            // camera's world Y, so the sign is opposite the horizontal anchor.
            NudgePanYCam (-((double) anchorY - (double) m_viewCenterY) * (double) m_dragPerPxY * s);
        }
    }

    NotifyChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPanZoom::NudgePanX
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanZoom::NudgePanX (double deltaContent)
{
    // panX.target is a continuous accumulator, so sub-notch touchpad deltas add
    // up naturally -- no whole-unit truncation to lose slow motion to.
    double target = m_panX.target + deltaContent;



    if (m_panXhi >= m_panXlo)
    {
        target = std::min (std::max (target, m_panXlo), m_panXhi);
    }

    if (target != m_panX.target)
    {
        m_panX.target = target;
        if (m_cfg.userPanInstant)
        {
            m_panX.cur = target;   // horizontal nudges are always user input
        }

        NotifyChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPanZoom::NudgePanY
//
//  Moves the vertical pan target by a content delta, spilling anything past
//  the bounds into overscroll.
//
//  The `user` flag distinguishes DIRECT MANIPULATION from programmatic follow,
//  and the two must not animate alike. A drag or wheel tracks the input
//  instantly -- easing under the user's finger reads as lag -- while a
//  programmatic move keeps the glide, so the snap back to the live row still
//  eases into place.
//
//  Only user motion fires the pan callback, which is how a caller
//  distinguishes "the user scrolled away" from its own repositioning and
//  avoids fighting itself.
//
//  Change is detected by comparing both the pan target AND the overscroll
//  target: a delta fully absorbed into overscroll moves nothing on the pan
//  axis but is still a visible change.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanZoom::NudgePanY (double deltaContent, bool user)
{
    double  prevPanY = m_panY.target;
    double  prevOver = m_overscrollY.target;
    bool    changed  = false;



    GetSpillPanY (deltaContent);

    changed = (m_panY.target != prevPanY) || (m_overscrollY.target != prevOver);

    // Direct manipulation tracks instantly; programmatic follow (user == false)
    // keeps the glide so the snap back to the live row still eases.
    if (user && m_cfg.userPanInstant)
    {
        m_panY.cur        = m_panY.target;
        m_overscrollY.cur = m_overscrollY.target;
    }

    if (user && m_onUserPanY)
    {
        m_onUserPanY();
    }

    if (changed)
    {
        NotifyChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPanZoom::GetSpillPanY
//
//  Apply a content delta to the panY target, spilling anything past the bounds
//  into the bounded overscroll offset. panY + overscroll behave as one extended
//  axis clamped to [lo - max, hi + max]: within [lo, hi] the overscroll stays
//  zero; beyond, panY pins at the bound and the remainder rides overscroll (so
//  panning back unwinds the overscroll before the paper scrolls again). With
//  overscrollMax = 0 this is just the old hard clamp.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanZoom::GetSpillPanY (double deltaContent)
{
    double  base  = m_panY.target + m_overscrollY.target;
    double  extLo = 0.0;
    double  extHi = 0.0;
    double  ext   = 0.0;



    if (m_panYhi < m_panYlo)   // bounds unset: free pan, no overscroll
    {
        m_panY.target        = base + deltaContent;
        m_overscrollY.target = 0.0;
        return;
    }

    extLo = m_panYlo - (double) m_overscrollMax;
    extHi = m_panYhi + (double) m_overscrollMax;
    ext = std::min (std::max (base + deltaContent, extLo), extHi);

    m_panY.target        = std::min (std::max (ext, m_panYlo), m_panYhi);
    m_overscrollY.target = ext - m_panY.target;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPanZoom::NudgePanYCam
//
//  Camera vertical framing (drag / cursor-anchored zoom). A continuous
//  accumulator clamped to the framing bounds, which grow with zoom -- moving
//  the eye over the magnified scene WITHOUT touching the content scroll or
//  follow mode. Snaps when direct manipulation is instant.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanZoom::NudgePanYCam (double deltaContent)
{
    double  target = m_panYCam.target + deltaContent;



    if (m_panYCamHi >= m_panYCamLo)
    {
        target = std::min (std::max (target, m_panYCamLo), m_panYCamHi);
    }

    if (target != m_panYCam.target)
    {
        m_panYCam.target = target;
        if (m_cfg.userPanInstant)
        {
            m_panYCam.cur = target;
        }

        NotifyChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPanZoom::ClampTargets
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanZoom::ClampTargets()
{
    if (m_panYhi >= m_panYlo)
    {
        m_panY.target = std::min (std::max (m_panY.target, m_panYlo), m_panYhi);
    }

    if (m_panXhi >= m_panXlo)
    {
        m_panX.target = std::min (std::max (m_panX.target, m_panXlo), m_panXhi);
    }

    if (m_panYCamHi >= m_panYCamLo)
    {
        m_panYCam.target = std::min (std::max (m_panYCam.target, m_panYCamLo), m_panYCamHi);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPanZoom::EaseToward
//
//  Advances one eased value toward its target, and reports whether it is still
//  moving.
//
//  The glide is exponential and FRAME-RATE INDEPENDENT: the step fraction is
//  1 - exp(-dt/tau), so the value follows the same curve in wall-clock time
//  whether frames arrive at 60 Hz or at 30. A fixed per-frame fraction would
//  make every animation run at the display's speed.
//
//  Because the step is recomputed from the current difference each tick, a
//  target that MOVES mid-glide is handled with no special case -- the value
//  simply chases wherever it now is.
//
//  Exponential decay never actually arrives, so a small epsilon snaps the last
//  fraction and ends the animation. Without it, `moving` would stay true
//  forever and the caller would render frames for a value that is visibly
//  finished.
//
//  A tau of zero means "no easing": the value snaps and reports not-moving,
//  which is what lets the same code path serve instant and animated modes.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPanZoom::EaseToward (Eased & v, double dtSec, double tauSec)
{
    double  diff   = v.target - v.cur;
    double  k      = 0.0;
    bool    moving = false;



    // Already there, or easing disabled (tau 0 means snap): either way the
    // value ends ON the target and nothing is still in flight.
    if (diff != 0.0 && tauSec <= 0.0)
    {
        v.cur = v.target;
    }
    else if (diff != 0.0)
    {
        // Frame-rate independent exponential glide toward a (possibly moving)
        // target.
        k      = 1.0 - exp (-dtSec / tauSec);
        v.cur += diff * k;

        // Close enough -> snap and stop animating.
        if (fabs (v.target - v.cur) < 0.01)
        {
            v.cur = v.target;
        }
        else
        {
            moving = true;
        }
    }

    return moving;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPanZoom::NotifyChanged
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanZoom::NotifyChanged()
{
    if (m_onChange)
    {
        m_onChange();
    }
}
