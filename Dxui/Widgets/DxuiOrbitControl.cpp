#include "Pch.h"
#include "Render/IDxuiPainter.h"

#include "DxuiOrbitControl.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous palette
//
//  Fixed ARGB rather than theme roles: the compass floats over the 3D scene
//  the way the HUD notice does, so it has to read against beige plastic,
//  black faceplates and the void alike -- the same reason the notice paints
//  its own shadow instead of trusting a theme background.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr uint32_t  s_kBackdropArgb = 0x2E000000;   // soft dark pool
static constexpr uint32_t  s_kIdleArgb     = 0x86FFFFFF;   // marks, unhovered
static constexpr uint32_t  s_kHoverArgb    = 0xD8FFFFFF;   // the part under the pointer
static constexpr uint32_t  s_kPressedArgb  = 0xFFFFFFFF;
static constexpr uint32_t  s_kOrbRimArgb   = 0x5AFFFFFF;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiOrbitControl::Metrics
//
////////////////////////////////////////////////////////////////////////////////

void DxuiOrbitControl::Metrics (float & cx, float & cy, float & orbR, float & reach) const
{
    RECT   rc   = GetBounds();
    float  side = (float) (std::min) (rc.right - rc.left, rc.bottom - rc.top);



    cx    = (rc.left + rc.right)  * 0.5f;
    cy    = (rc.top  + rc.bottom) * 0.5f;
    orbR  = side * 0.16f;
    reach = side * 0.46f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiOrbitControl::HitPart
//
//  The orb by radius, the arrows by quadrant. The whole disc is live, not
//  just the drawn triangles: a discoverability control with fussy targets
//  teaches the user that it does not work.
//
////////////////////////////////////////////////////////////////////////////////

DxuiOrbitControl::Part DxuiOrbitControl::HitPart (int xPx, int yPx) const
{
    float  cx    = 0.0f;
    float  cy    = 0.0f;
    float  orbR  = 0.0f;
    float  reach = 0.0f;
    float  dx    = 0.0f;
    float  dy    = 0.0f;
    float  dist  = 0.0f;



    if (!IsVisible())
    {
        return Part::None;
    }

    Metrics (cx, cy, orbR, reach);

    dx   = (float) xPx - cx;
    dy   = (float) yPx - cy;
    dist = std::sqrt (dx * dx + dy * dy);

    if (dist > reach)
    {
        return Part::None;
    }

    if (dist <= orbR * 1.25f)
    {
        return Part::Orb;
    }

    // Quadrants by the diagonals: whichever axis dominates names the arrow.
    if (std::abs (dx) >= std::abs (dy))
    {
        return (dx >= 0.0f) ? Part::Right : Part::Left;
    }

    return (dy >= 0.0f) ? Part::Down : Part::Up;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiOrbitControl::OnPointerDown
//
//  Click against drag is settled the way buttons settle it everywhere:
//  travel past kClickSlopPx makes it a drag and it can never become a click
//  again; release inside the slop is the click. The drag reports deltas
//  since the LAST report, so the caller integrates and this control keeps
//  no rotation state of its own.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiOrbitControl::OnPointerDown (int xPx, int yPx)
{
    Part  part = HitPart (xPx, yPx);



    if (part == Part::None)
    {
        return false;
    }

    m_armed         = true;
    m_dragging      = false;
    m_armed_on      = part;
    m_pressPx       = POINT { xPx, yPx };
    m_lastPx        = m_pressPx;
    m_repeatBlocked = false;
    m_repeatAtMs    = 0;

    // The arrow turns NOW. The orb does not: home discards the framing the
    // user has built, and it keeps the release so a press can be dragged off
    // and abandoned.
    if (part != Part::Orb && m_onStep)
    {
        m_onStep (part);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiOrbitControl::OnPointerMove
//
//  Armed, it owns the gesture; idle, it keeps the hover honest.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiOrbitControl::OnPointerMove (int xPx, int yPx)
{
    if (!m_armed)
    {
        Part  over = HitPart (xPx, yPx);

        if (over != m_hover)
        {
            m_hover = over;
        }

        return false;
    }

    if (!m_dragging &&
        std::abs (xPx - m_pressPx.x) <= kClickSlopPx &&
        std::abs (yPx - m_pressPx.y) <= kClickSlopPx)
    {
        return true;
    }

    // Moving means aiming, and aiming outranks repeating. Latched for the
    // rest of the press: going still again mid-drag must not start firing
    // steps into the gesture the hand is still making.
    m_dragging      = true;
    m_repeatBlocked = true;

    // The orb is a click target only -- dragging from it is nothing, rather
    // than a free rotate that would make the arrows' axis lock look broken
    // by comparison.
    if (m_armed_on != Part::Orb && m_onDrag)
    {
        m_onDrag (m_armed_on,
                  (float) (xPx - m_lastPx.x),
                  (float) (yPx - m_lastPx.y));
    }

    m_lastPx = POINT { xPx, yPx };

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiOrbitControl::OnPointerUp
//
//  Release inside the slop is the click; a drag simply ends.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiOrbitControl::OnPointerUp (int xPx, int yPx)
{
    bool  wasArmed = m_armed;



    UNREFERENCED_PARAMETER (xPx);
    UNREFERENCED_PARAMETER (yPx);

    if (!wasArmed)
    {
        return false;
    }

    m_armed = false;

    // ONLY THE ORB ACTS HERE. An arrow already turned the scene on the way
    // down and may have gone on turning it since; firing again on release
    // would make every click worth two steps.
    if (!m_dragging && m_armed_on == Part::Orb && m_onHome)
    {
        m_onHome();
    }

    m_dragging      = false;
    m_repeatBlocked = false;
    m_repeatAtMs    = 0;
    m_armed_on      = Part::None;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiOrbitControl::Tick
//
//  The held arrow's repeat. Scheduled on the first tick after the press
//  rather than in OnPointerDown, which is what keeps the clock out of the
//  pointer handlers -- the caller owns the time here as it does for the
//  chrome tooltips.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiOrbitControl::Tick (int64_t nowMs)
{
    if (!WantsTick())
    {
        return;
    }

    if (m_repeatAtMs == 0)
    {
        m_repeatAtMs = nowMs + kRepeatDelayMs;
        return;
    }

    if (nowMs < m_repeatAtMs)
    {
        return;
    }

    m_repeatAtMs = nowMs + kRepeatIntervalMs;

    if (m_onStep)
    {
        m_onStep (m_armed_on);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiOrbitControl::Layout
//
////////////////////////////////////////////////////////////////////////////////

void DxuiOrbitControl::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    m_dpi = scaler.GetDpi();
    SetBounds (boundsDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiOrbitControl::Paint
//
//  Backdrop pool, orb, four arrows. Every mark is built from the same
//  center/orb/reach metrics the hit test uses, so what highlights is what
//  the click will do -- the two cannot drift apart because there is only
//  one geometry.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiOrbitControl::Paint (IDxuiPainter      & painter,
                              IDxuiTextRenderer & text,
                              const IDxuiTheme  & theme)
{
    float  cx    = 0.0f;
    float  cy    = 0.0f;
    float  orbR  = 0.0f;
    float  reach = 0.0f;



    UNREFERENCED_PARAMETER (text);
    UNREFERENCED_PARAMETER (theme);

    if (!IsVisible())
    {
        return;
    }

    Metrics (cx, cy, orbR, reach);

    // The pool: two circles, the outer fainter, a cheap two-ring feather of
    // the HUD notice's shadow idea.
    painter.FillCircleApprox (cx, cy, reach,         s_kBackdropArgb);
    painter.FillCircleApprox (cx, cy, reach * 0.82f, s_kBackdropArgb);

    // The orb, with a rim so it reads as a thing rather than a dot.
    {
        bool      hot  = (m_hover == Part::Orb) || (m_armed && m_armed_on == Part::Orb);
        uint32_t  fill = m_armed && m_armed_on == Part::Orb ? s_kPressedArgb
                       : hot                                ? s_kHoverArgb
                                                            : s_kIdleArgb;

        painter.FillCircleApprox (cx, cy, orbR * 1.18f, s_kOrbRimArgb);
        painter.FillCircleApprox (cx, cy, orbR,          fill);
    }

    // The arrows: an isoceles triangle per compass point, base toward the
    // orb, apex outward. One parametric shape, rotated by axis swaps.
    {
        struct Arrow
        {
            Part   part;
            float  ux;      // unit vector, center toward apex
            float  uy;
        };

        constexpr Arrow  arrows[] =
        {
            { Part::Up,     0.0f, -1.0f },
            { Part::Down,   0.0f,  1.0f },
            { Part::Left,  -1.0f,  0.0f },
            { Part::Right,  1.0f,  0.0f },
        };

        for (const Arrow & a : arrows)
        {
            bool      hot   = (m_hover == a.part) || (m_armed && m_armed_on == a.part);
            uint32_t  fill  = m_armed && m_armed_on == a.part ? s_kPressedArgb
                            : hot                             ? s_kHoverArgb
                                                              : s_kIdleArgb;
            float     base  = orbR * 1.55f;                    // inner edge, off the orb
            float     apex  = reach * 0.92f;                   // outer point
            float     halfW = orbR * 0.85f;                    // half the base width
            float     px    = -a.uy;                           // perpendicular
            float     py    = a.ux;

            float     bx    = cx + a.ux * base;
            float     by    = cy + a.uy * base;
            float     ax    = cx + a.ux * apex;
            float     ay    = cy + a.uy * apex;

            painter.FillConvexQuad (bx + px * halfW, by + py * halfW,
                                    ax,              ay,
                                    ax,              ay,
                                    bx - px * halfW, by - py * halfW,
                                    fill);
        }
    }
}
