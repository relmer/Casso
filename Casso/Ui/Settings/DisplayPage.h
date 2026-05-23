#pragma once

#include "Pch.h"

#include "../DwriteTextRenderer.h"
#include "../DxUiPainter.h"
#include "../Widgets/Label.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPage
//
//  Placeholder page. CRT brightness + per-effect controls land in P4
//  alongside the global-prefs persistence rework.
//
////////////////////////////////////////////////////////////////////////////////

class DisplayPage
{
public:
    void  Layout (const RECT & rect)
    {
        RECT  msgRect = { rect.left + 16, rect.top + 16, rect.right - 16, rect.top + 48 };
        m_placeholder.SetRect (msgRect);
        m_placeholder.SetText (L"Display + CRT controls arrive in P4.");
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
