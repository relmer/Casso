#include "Pch.h"

#include "Core/DxuiPanZoom.h"

#include "Core/DxuiInput.h"




DxuiPanZoom::DxuiPanZoom (const Config & cfg)
    : m_cfg (cfg)
{
    m_zoom.cur    = cfg.zoomMin;
    m_zoom.target = cfg.zoomMin;
}




bool DxuiPanZoom::OnMouse (const DxuiMouseEvent & ev)
{
    switch (ev.kind)
    {
    case DxuiMouseEventKind::Wheel:
        {
            if (ev.wheelDelta == 0.0f)
            {
                return false;
            }

            // Ctrl + wheel zooms (this is also how a Precision Touchpad pinch
            // arrives). Vertical wheel only -- a horizontal pinch is nonsense.
            // Anchored on the cursor so the content under it stays fixed.
            if (ev.ctrl && !ev.wheelHorizontal && m_cfg.enableZoom)
            {
                ApplyZoomFactor (ev.wheelDelta > 0.0f ? m_cfg.zoomStep : 1.0 / m_cfg.zoomStep,
                                 /*anchored*/ true,
                                 (float) ev.positionDip.x, (float) ev.positionDip.y);
                return true;
            }

            if (ev.wheelHorizontal)
            {
                if (! m_cfg.enablePanX)
                {
                    return false;
                }
                NudgePanX ((double) ev.wheelDelta * m_cfg.wheelPanX);
                return true;
            }

            if (! m_cfg.enablePanY)
            {
                return false;
            }
            // Wheel up (+delta) reveals earlier content -> pan target DECREASES;
            // the caller decides sign meaning via bounds, we keep +wheel = -pan
            // so "scroll up = go back" matches every other scroll surface.
            NudgePanY (-(double) ev.wheelDelta * m_cfg.wheelPanY, /*user*/ true);
            return true;
        }

    case DxuiMouseEventKind::Down:
        if (ev.button == DxuiMouseButton::Left && m_cfg.enableDrag)
        {
            m_dragging = true;
            m_dragLast = ev.positionDip;
            return true;
        }
        return false;

    case DxuiMouseEventKind::Move:
        if (m_dragging)
        {
            double dx = (double) (ev.positionDip.x - m_dragLast.x);
            double dy = (double) (ev.positionDip.y - m_dragLast.y);
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
            return true;
        }
        return false;

    case DxuiMouseEventKind::Up:
        if (m_dragging && ev.button == DxuiMouseButton::Left)
        {
            m_dragging = false;
            return true;
        }
        return false;

    default:
        return false;
    }
}




bool DxuiPanZoom::OnKey (const DxuiKeyEvent & ev)
{
    if (ev.kind != DxuiKeyEventKind::Down || ! ev.ctrl || ! m_cfg.enableZoom)
    {
        return false;
    }

    switch (ev.vk)
    {
    case VK_OEM_PLUS:
    case VK_ADD:
        ApplyZoomFactor (m_cfg.zoomStep);
        return true;

    case VK_OEM_MINUS:
    case VK_SUBTRACT:
        ApplyZoomFactor (1.0 / m_cfg.zoomStep);
        return true;

    case '0':
    case VK_NUMPAD0:
        ResetZoom ();
        return true;

    default:
        return false;
    }
}




bool DxuiPanZoom::Tick (double nowSec)
{
    double dt = (m_lastTickSec < 0.0) ? 0.0 : (nowSec - m_lastTickSec);
    m_lastTickSec = nowSec;

    if (dt <= 0.0)
    {
        return (m_zoom.cur != m_zoom.target) || (m_panX.cur != m_panX.target) ||
               (m_panY.cur != m_panY.target) || (m_panYCam.cur != m_panYCam.target) ||
               (m_overscrollY.cur != m_overscrollY.target);
    }

    bool moving = false;
    moving |= EaseToward (m_zoom, dt, m_cfg.zoomEaseTauSec);
    moving |= EaseToward (m_panX, dt, m_cfg.easeTauSec);
    moving |= EaseToward (m_panY, dt, m_cfg.easeTauSec);
    moving |= EaseToward (m_panYCam, dt, m_cfg.easeTauSec);
    moving |= EaseToward (m_overscrollY, dt, m_cfg.easeTauSec);

    if (moving)
    {
        Changed ();
    }
    return moving;
}




void DxuiPanZoom::SetPanYBounds (float lo, float hi)
{
    m_panYlo = lo;
    m_panYhi = hi;
    ClampTargets ();
}




void DxuiPanZoom::SetPanXBounds (float lo, float hi)
{
    m_panXlo = lo;
    m_panXhi = hi;
    ClampTargets ();
}




void DxuiPanZoom::SetPanYCamBounds (float lo, float hi)
{
    m_panYCamLo = lo;
    m_panYCamHi = hi;
    ClampTargets ();
}




void DxuiPanZoom::SetDragScale (float contentPerPixelX, float contentPerPixelY)
{
    m_dragPerPxX = contentPerPixelX;
    m_dragPerPxY = contentPerPixelY;
}




void DxuiPanZoom::SetPanYTarget (float y)
{
    double clamped = y;
    if (m_panYhi >= m_panYlo)
    {
        clamped = std::min (std::max ((double) y, m_panYlo), m_panYhi);
    }

    // Follow mode owns the paper position again, so spring any world overscroll
    // back home (eases via Tick), returning the view to its resting frame.
    bool  changed = (clamped != m_panY.target) || (m_overscrollY.target != 0.0);

    m_panY.target        = clamped;
    m_overscrollY.target = 0.0;

    if (changed)
    {
        Changed ();
    }
}




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




void DxuiPanZoom::SnapPanY (float y)
{
    m_panY.cur    = y;
    m_panY.target = y;
    m_overscrollY.cur    = 0.0;   // torn / replaced content: world back to home
    m_overscrollY.target = 0.0;
    ClampTargets ();
    Changed ();
}




void DxuiPanZoom::ZoomIn ()
{
    ApplyZoomFactor (m_cfg.zoomStep);
}




void DxuiPanZoom::ZoomOut ()
{
    ApplyZoomFactor (1.0 / m_cfg.zoomStep);
}




void DxuiPanZoom::ResetZoom ()
{
    if (m_zoom.target != (double) m_cfg.zoomMin)
    {
        m_zoom.target = m_cfg.zoomMin;
        Changed ();
    }
}




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

    Changed ();
}




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
        Changed ();
    }
}




void DxuiPanZoom::NudgePanY (double deltaContent, bool user)
{
    double  prevPanY = m_panY.target;
    double  prevOver = m_overscrollY.target;

    SpillPanY (deltaContent);

    bool  changed = (m_panY.target != prevPanY) || (m_overscrollY.target != prevOver);

    // Direct manipulation tracks instantly; programmatic follow (user == false)
    // keeps the glide so the snap back to the live row still eases.
    if (user && m_cfg.userPanInstant)
    {
        m_panY.cur        = m_panY.target;
        m_overscrollY.cur = m_overscrollY.target;
    }

    if (user && m_onUserPanY)
    {
        m_onUserPanY ();
    }
    if (changed)
    {
        Changed ();
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPanZoom::SpillPanY
//
//  Apply a content delta to the panY target, spilling anything past the bounds
//  into the bounded overscroll offset. panY + overscroll behave as one extended
//  axis clamped to [lo - max, hi + max]: within [lo, hi] the overscroll stays
//  zero; beyond, panY pins at the bound and the remainder rides overscroll (so
//  panning back unwinds the overscroll before the paper scrolls again). With
//  overscrollMax = 0 this is just the old hard clamp.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPanZoom::SpillPanY (double deltaContent)
{
    double  base = m_panY.target + m_overscrollY.target;

    if (m_panYhi < m_panYlo)   // bounds unset: free pan, no overscroll
    {
        m_panY.target        = base + deltaContent;
        m_overscrollY.target = 0.0;
        return;
    }

    double  extLo = m_panYlo - (double) m_overscrollMax;
    double  extHi = m_panYhi + (double) m_overscrollMax;
    double  ext   = std::min (std::max (base + deltaContent, extLo), extHi);

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
        Changed ();
    }
}




void DxuiPanZoom::ClampTargets ()
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




bool DxuiPanZoom::EaseToward (Eased & v, double dtSec, double tauSec)
{
    double diff = v.target - v.cur;
    if (diff == 0.0)
    {
        return false;
    }

    if (tauSec <= 0.0)
    {
        v.cur = v.target;
        return false;
    }

    // Frame-rate independent exponential glide toward a (possibly moving) target.
    double k = 1.0 - exp (-dtSec / tauSec);
    v.cur += diff * k;

    // Close enough -> snap and stop animating.
    if (fabs (v.target - v.cur) < 0.01)
    {
        v.cur = v.target;
        return false;
    }
    return true;
}




void DxuiPanZoom::Changed ()
{
    if (m_onChange)
    {
        m_onChange ();
    }
}
