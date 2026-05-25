#pragma once

#include "Pch.h"

#include "../DpiScaler.h"
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
    void  Layout (const RECT & rect, const DpiScaler & scaler)
    {
        int   pad     = scaler.Px (16);
        int   bottom  = scaler.Px (48);
        RECT  msgRect = { rect.left + pad, rect.top + pad, rect.right - pad, rect.top + bottom };
        m_placeholder.SetRect (msgRect);
        m_placeholder.SetText (L"Display + CRT controls arrive in P4.");
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
