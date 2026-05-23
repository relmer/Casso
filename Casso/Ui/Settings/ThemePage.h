#pragma once

#include "Pch.h"

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
    void  Layout (const RECT & rect)
    {
        RECT  msgRect = { rect.left + 16, rect.top + 16, rect.right - 16, rect.top + 48 };
        m_placeholder.SetRect (msgRect);
        m_placeholder.SetText (L"Theme picker arrives in P4.");
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
