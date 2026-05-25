#pragma once

#include "Pch.h"

#include "../DpiScaler.h"
#include "../DwriteTextRenderer.h"
#include "../DxUiPainter.h"
#include "../Widgets/Label.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ThemePage
//
//  Placeholder page. The real theme picker + hot-swap pipeline lands
//  in P4. P3 ships an inert page so the surrounding TabStrip + apply
//  pipeline can be wired end-to-end and screenshot-tested without a
//  trailing missing-tab.
//
////////////////////////////////////////////////////////////////////////////////

class ThemePage
{
public:
    void  Layout (const RECT & rect, const DpiScaler & scaler)
    {
        int   pad     = scaler.Px (16);
        int   bottom  = scaler.Px (48);
        RECT  msgRect = { rect.left + pad, rect.top + pad, rect.right - pad, rect.top + bottom };
        m_placeholder.SetRect (msgRect);
        m_placeholder.SetText (L"Theme picker arrives in P4.");
        m_placeholder.SetDpi  (scaler.Dpi());
    }

    void  OnLButtonDown (int, int) {}
    void  OnLButtonUp   (int, int) {}
    void  OnMouseHover  (int, int) {}
    bool  OnKey         (WPARAM)   { return false; }

    void  Paint (DxUiPainter & painter, DwriteTextRenderer & text) const
    {
        m_placeholder.Paint (painter, text);
    }

private:
    Label  m_placeholder;
};
