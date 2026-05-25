#pragma once

#include "Pch.h"

#include "../DpiScaler.h"
#include "../DwriteTextRenderer.h"
#include "../DxUiPainter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TabStrip
//
//  Horizontal tab selector. Owns an ordered list of (label, rect)
//  tabs and a single selected index. Mouse activates whichever tab
//  the click lands in; keyboard cycles Left / Right with wrap.
//
////////////////////////////////////////////////////////////////////////////////

class TabStrip
{
public:
    using ChangeFn = std::function<void (int newIndex)>;

    struct Tab
    {
        RECT          rect = {};
        std::wstring  label;
    };

    void  SetTabs    (std::vector<Tab> tabs) { m_tabs = std::move (tabs); }
    void  SetSelected (int index);
    void  SetEnabled (bool enabled) { m_enabled = enabled; }
    void  SetFocused (bool focused) { m_focused = focused; }
    void  SetOnChange (ChangeFn fn) { m_change = std::move (fn); }

    const std::vector<Tab> & Tabs       () const { return m_tabs;    }
    int                      Selected   () const { return m_selected; }
    int                      HoverIndex () const { return m_hover;   }
    bool                     Enabled    () const { return m_enabled; }
    bool                     Focused    () const { return m_focused; }

    int   HitTest        (int x, int y) const;
    void  SetMouseHover  (int x, int y);
    bool  OnLButtonDown  (int x, int y);
    bool  OnLButtonUp    (int x, int y);
    bool  OnKey          (WPARAM vk);

    void  Paint          (DxUiPainter & painter, DwriteTextRenderer & text) const;
    void  SetDpi         (UINT dpi) { m_scaler.SetDpi (dpi); }

private:
    void  Commit (int newIndex);


    std::vector<Tab>  m_tabs;
    ChangeFn          m_change;
    int               m_selected = 0;
    int               m_hover    = -1;
    int               m_pressed  = -1;
    bool              m_enabled  = true;
    bool              m_focused  = false;
    DpiScaler         m_scaler;
};
