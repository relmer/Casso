#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"






////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip
//
//  Horizontal tab selector. Owns an ordered list of (label, rect)
//  tabs and a single selected index. Mouse activates whichever tab
//  the click lands in; keyboard cycles Left / Right with wrap.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTabStrip : public IDxuiControl
{
public:
    using ChangeFn = std::function<void (int newIndex)>;

    struct Tab
    {
        RECT          rect = {};
        std::wstring  label;
    };

    ~DxuiTabStrip() override = default;

    void  SetTabs    (std::vector<Tab> tabs) { m_tabs = std::move (tabs); }
    void  SetSelected (int index);
    void  SetEnabled (bool enabled) { IDxuiControl::SetEnabled (enabled); m_enabled = enabled; }
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

    void  Paint          (IDxuiPainter & painter, IDxuiTextRenderer & text) const;
    void  SetDpi         (UINT dpi) { m_scaler.SetDpi (dpi); }

    //
    //  IDxuiControl overrides — additive shims for DxuiPanel trees.
    //
    void                Layout         (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint          (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse        (const DxuiMouseEvent & ev) override;
    bool                OnKey          (const DxuiKeyEvent   & ev) override;
    void                OnFocusChanged (bool focused) override { SetFocused (focused); }
    std::wstring        AccessibleName () const override;
    DxuiAccessibleRole  AccessibleRole () const override { return DxuiAccessibleRole::TabStrip; }

private:
    void  Commit (int newIndex);
    void  PaintInternal (IDxuiPainter & painter, IDxuiTextRenderer & text,
                         uint32_t idleArgb, uint32_t hoverArgb, uint32_t selectedArgb,
                         uint32_t textArgb, uint32_t focusArgb) const;


    std::vector<Tab>  m_tabs;
    ChangeFn          m_change;
    int               m_selected = 0;
    int               m_hover    = -1;
    int               m_pressed  = -1;
    bool              m_enabled  = true;
    bool              m_focused  = false;
    DxuiDpiScaler         m_scaler;
};
