#include "Pch.h"

#include "DxuiSystemButton.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"



namespace
{
    // Glyph stroke thickness and inset within the button rect, in DIPs.
    constexpr float  s_kGlyphInsetDip      = 6.0f;
    constexpr float  s_kGlyphThicknessDip  = 1.0f;
    constexpr float  s_kRestoreOffsetDip   = 3.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSystemButton
//
//  Constructor — records the button kind. HWND is set later by the
//  host window once it owns a real handle.
//
////////////////////////////////////////////////////////////////////////////////

DxuiSystemButton::DxuiSystemButton (DxuiSystemButtonKind kind)
{
    m_kind      = kind;
    m_focusable = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetHwnd
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSystemButton::SetHwnd (HWND hwnd)
{
    DXUI_ASSERT_UI_THREAD();

    m_hwnd = hwnd;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
//  Stores the bounds and snapshots the DPI scaler for use during
//  glyph rendering.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSystemButton::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    DXUI_ASSERT_UI_THREAD();

    SetBounds (boundsDip);
    m_scaler = scaler;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
//  Hover / pressed background, then a vector glyph appropriate for
//  the button kind. Close uses a distinct hover colour
//  (SystemCloseHover) per Win11 chrome guidelines.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSystemButton::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT     bounds      = {};
    float    xPx         = 0.0f;
    float    yPx         = 0.0f;
    float    widthPx     = 0.0f;
    float    heightPx    = 0.0f;
    float    insetPx     = 0.0f;
    float    glyphLeft   = 0.0f;
    float    glyphTop    = 0.0f;
    float    glyphRight  = 0.0f;
    float    glyphBottom = 0.0f;
    float    midX            = 0.0f;
    float    midY            = 0.0f;
    float    strokePx        = 0.0f;
    float    restoreOffsetPx = 0.0f;
    uint32_t fg              = 0;



    DXUI_ASSERT_UI_THREAD();

    (void) text;

    bounds   = Bounds();
    xPx      = (float) m_scaler.Px (bounds.left);
    yPx      = (float) m_scaler.Px (bounds.top);
    widthPx  = (float) m_scaler.Px (bounds.right  - bounds.left);
    heightPx = (float) m_scaler.Px (bounds.bottom - bounds.top);

    // Hover / pressed background fill.
    if (m_hovered || m_pressed)
    {
        uint32_t bg = 0;

        if (m_kind == DxuiSystemButtonKind::Close)
        {
            bg = m_pressed ? theme.SystemClosePressed() : theme.SystemCloseHover();
        }
        else
        {
            bg = m_pressed ? theme.SystemButtonPressed() : theme.SystemButtonHover();
        }
        painter.FillRect (xPx, yPx, widthPx, heightPx, bg);
    }

    // Glyph foreground — close-on-hover uses white for contrast against red.
    fg = theme.CaptionForeground();
    if (m_kind == DxuiSystemButtonKind::Close && (m_hovered || m_pressed))
    {
        fg = 0xFFFFFFFF;
    }

    insetPx     = m_scaler.Pxf (s_kGlyphInsetDip);
    strokePx    = m_scaler.Pxf (s_kGlyphThicknessDip);
    if (strokePx < 1.0f)
    {
        strokePx = 1.0f;
    }

    glyphLeft   = xPx + insetPx;
    glyphTop    = yPx + insetPx;
    glyphRight      = xPx + widthPx  - insetPx;
    glyphBottom     = yPx + heightPx - insetPx;
    midX            = (glyphLeft + glyphRight) * 0.5f;
    midY            = (glyphTop  + glyphBottom) * 0.5f;
    restoreOffsetPx = m_scaler.Pxf (s_kRestoreOffsetDip);

    switch (m_kind)
    {
        case DxuiSystemButtonKind::Min:
            // Single horizontal line through vertical center.
            painter.FillRect (glyphLeft,
                              midY - strokePx * 0.5f,
                              glyphRight - glyphLeft,
                              strokePx,
                              fg);
            break;

        case DxuiSystemButtonKind::Max:
            if (m_maximized)
            {
                painter.OutlineRect (glyphLeft + restoreOffsetPx,
                                     glyphTop,
                                     glyphRight - glyphLeft - restoreOffsetPx,
                                     glyphBottom - glyphTop - restoreOffsetPx,
                                     strokePx,
                                     fg);
                painter.OutlineRect (glyphLeft,
                                     glyphTop + restoreOffsetPx,
                                     glyphRight - glyphLeft - restoreOffsetPx,
                                     glyphBottom - glyphTop - restoreOffsetPx,
                                     strokePx,
                                     fg);
            }
            else
            {
                painter.OutlineRect (glyphLeft,
                                     glyphTop,
                                     glyphRight  - glyphLeft,
                                     glyphBottom - glyphTop,
                                     strokePx,
                                     fg);
            }
            break;

        case DxuiSystemButtonKind::Close:
            // Two crossed diagonals. The painter exposes axis-aligned
            // primitives only, so emit a small grid of pixel-thin quads
            // along each diagonal. Quick, no-dependency approach; good
            // enough for chrome at typical button sizes (~32 DIP).
            {
                float  span = glyphRight - glyphLeft;
                int    steps = (int) span;
                int    i     = 0;

                if (steps < 1)
                {
                    steps = 1;
                }

                for (i = 0; i < steps; ++i)
                {
                    float  t  = (float) i / (float) steps;
                    float  px = glyphLeft + t * span;
                    float  py = glyphTop  + t * (glyphBottom - glyphTop);

                    painter.FillRect (px, py, strokePx, strokePx, fg);

                    float  px2 = glyphRight - t * span;
                    painter.FillRect (px2, py, strokePx, strokePx, fg);
                }
            }
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouse
//
//  Track hover / press; dispatch system command on release inside
//  bounds. Caller (host WndProc) already filtered by hit-test before
//  routing the event here.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSystemButton::OnMouse (const DxuiMouseEvent & ev)
{
    RECT  bounds  = {};
    bool  inside  = false;



    DXUI_ASSERT_UI_THREAD();

    bounds = Bounds();
    inside = (ev.positionDip.x >= bounds.left && ev.positionDip.x < bounds.right &&
              ev.positionDip.y >= bounds.top  && ev.positionDip.y < bounds.bottom);

    switch (ev.kind)
    {
        case DxuiMouseEventKind::Enter:
        case DxuiMouseEventKind::Move:
            m_hovered = inside;
            return false;

        case DxuiMouseEventKind::Leave:
            m_hovered = false;
            m_pressed = false;
            return false;

        case DxuiMouseEventKind::Down:
            if (inside && ev.button == DxuiMouseButton::Left)
            {
                m_pressed = true;
                return true;
            }
            return false;

        case DxuiMouseEventKind::Up:
            if (m_pressed && ev.button == DxuiMouseButton::Left)
            {
                m_pressed = false;
                if (inside)
                {
                    DispatchClick();
                    return true;
                }
            }
            return false;

        case DxuiMouseEventKind::Wheel:
            return false;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClassifyHit
//
//  Reports the matching DxuiHitTestKind so the host WndProc can
//  translate to HTMINBUTTON / HTMAXBUTTON / HTCLOSE. The MaxButton
//  classification is what unlocks the Win11 snap-layouts hover
//  popover.
//
////////////////////////////////////////////////////////////////////////////////

DxuiHitTestKind DxuiSystemButton::ClassifyHit (POINT clientDip) const
{
    (void) clientDip;

    switch (m_kind)
    {
        case DxuiSystemButtonKind::Min:   return DxuiHitTestKind::MinButton;
        case DxuiSystemButtonKind::Max:   return DxuiHitTestKind::MaxButton;
        case DxuiSystemButtonKind::Close: return DxuiHitTestKind::CloseButton;
    }
    return DxuiHitTestKind::Client;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessibleName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiSystemButton::AccessibleName () const
{
    switch (m_kind)
    {
        case DxuiSystemButtonKind::Min:   return L"Minimize";
        case DxuiSystemButtonKind::Max:   return L"Maximize";
        case DxuiSystemButtonKind::Close: return L"Close";
    }
    return L"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DispatchClick
//
//  Sends the matching Win32 system command for this button kind to
//  the associated HWND. No-op if the HWND is unset (e.g. tests).
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSystemButton::DispatchClick ()
{
    if (m_hwnd == nullptr)
    {
        return;
    }

    switch (m_kind)
    {
        case DxuiSystemButtonKind::Min:
            ShowWindow (m_hwnd, SW_MINIMIZE);
            break;

        case DxuiSystemButtonKind::Max:
            SendMessage (m_hwnd,
                         WM_SYSCOMMAND,
                         IsZoomed (m_hwnd) ? SC_RESTORE : SC_MAXIMIZE,
                         0);
            break;

        case DxuiSystemButtonKind::Close:
            SendMessage (m_hwnd, WM_CLOSE, 0, 0);
            break;
    }
}
