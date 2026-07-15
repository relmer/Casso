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
            if (ev.ctrl && !ev.wheelHorizontal && m_cfg.enableZoom)
            {
                ApplyZoomFactor (ev.wheelDelta > 0.0f ? m_cfg.zoomStep : 1.0 / m_cfg.zoomStep);
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

            // Dragging the content right/down should move the content WITH the
            // cursor, i.e. reveal what is up/left -> pan target moves opposite.
            if (m_cfg.enablePanX && dx != 0.0)
            {
                NudgePanX (-dx * (double) m_dragPerPxX);
            }
            if (m_cfg.enablePanY && dy != 0.0)
            {
                NudgePanY (-dy * (double) m_dragPerPxY, /*user*/ true);
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
        return (m_zoom.cur != m_zoom.target) || (m_panX.cur != m_panX.target) || (m_panY.cur != m_panY.target);
    }

    bool moving = false;
    moving |= EaseToward (m_zoom, dt, m_cfg.zoomEaseTauSec);
    moving |= EaseToward (m_panX, dt, m_cfg.easeTauSec);
    moving |= EaseToward (m_panY, dt, m_cfg.easeTauSec);

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
    if (clamped != m_panY.target)
    {
        m_panY.target = clamped;
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




void DxuiPanZoom::ApplyZoomFactor (double factor)
{
    double z = std::min (std::max (m_zoom.target * factor, (double) m_cfg.zoomMin), (double) m_cfg.zoomMax);
    if (z != m_zoom.target)
    {
        m_zoom.target = z;
        Changed ();
    }
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
    double target = m_panY.target + deltaContent;
    if (m_panYhi >= m_panYlo)
    {
        target = std::min (std::max (target, m_panYlo), m_panYhi);
    }

    bool changed = (target != m_panY.target);
    m_panY.target = target;

    // Direct manipulation tracks instantly; programmatic follow (user == false)
    // keeps the glide so the snap back to the live row still eases.
    if (user && m_cfg.userPanInstant)
    {
        m_panY.cur = target;
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
