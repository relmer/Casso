#pragma once

#include "Pch.h"

#include "../Chrome/ChromeTheme.h"
#include "../DpiScaler.h"
#include "../DwriteTextRenderer.h"
#include "../DxUiPainter.h"





class Button
{
public:
    using ClickFn = std::function<void()>;

    void  Layout          (const RECT & rect) { m_rect = rect; }
    void  SetLabel        (const std::wstring & label) { m_label = label; }
    void  SetClick        (ClickFn click) { m_click = std::move (click); }
    void  SetDpi          (UINT dpi) { m_scaler.SetDpi (dpi); }
    void  SetMouse        (int x, int y, bool down);
    bool  HitTest         (int x, int y) const;
    void  Click           ();
    void  Paint           (DxUiPainter & painter, DwriteTextRenderer & text, const ChromeTheme & theme);

private:
    RECT          m_rect    = {};
    std::wstring  m_label;
    ClickFn       m_click;
    bool          m_hover   = false;
    bool          m_pressed = false;
    DpiScaler     m_scaler;
};
