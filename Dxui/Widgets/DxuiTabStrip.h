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
//  Tabs past the strip's edge scroll into reach, by the wheel, by being
//  selected, or by a drag held past either end. A tab dragged along the
//  strip moves as it crosses its neighbors, and each move is reported.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTabStrip : public IDxuiControl
{
public:
    using ChangeFn = std::function<void (int newIndex)>;
    using MoveFn   = std::function<void (int from, int to)>;

    DxuiTabStrip() { m_focusable = true; }

    struct Tab
    {
        RECT          rect = {};
        std::wstring  label;
    };

    ~DxuiTabStrip() override = default;

    void  SetTabs    (std::vector<Tab> tabs);
    void  SetSelected (int index);
    void  SetEnabled (bool enabled) { IDxuiControl::SetEnabled (enabled); m_enabled = enabled; }
    void  SetFocused (bool focused) { m_focused = focused; }
    void  SetOnChange (ChangeFn fn) { m_change = std::move (fn); }
    void  SetOnMove   (MoveFn fn)   { m_move   = std::move (fn); }

    const std::vector<Tab> & GetTabs       () const { return m_tabs;    }
    int                      GetSelected   () const { return m_selected; }
    int                      GetHoverIndex () const { return m_hover;   }
    bool                     IsEnabled     () const { return m_enabled; }
    bool                     IsFocused     () const { return m_focused; }
    int                      GetScrollPx   () const { return m_scrollPx; }

    //  True from a press on a tab until the button comes up, so the host can
    //  keep sending moves to a drag that has left the strip.
    bool                     IsInteracting () const { return m_pressed >= 0; }

    int   HitTest        (int x, int y) const;
    void  SetMouseHover  (int x, int y);
    bool  OnLButtonDown  (int x, int y);
    bool  OnLButtonUp    (int x, int y);
    bool  OnMouseMove    (int x, int y);
    bool  OnWheel        (float notches);
    bool  OnKey          (WPARAM vk);

    void  Paint          (IDxuiPainter & painter, IDxuiTextRenderer & text) const;
    void  SetDpi         (UINT dpi) { m_scaler.SetDpi (dpi); }

    //
    //  IDxuiControl overrides — additive shims for DxuiPanel trees.
    //
    void                Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;
    bool                OnKey             (const DxuiKeyEvent   & ev) override;
    void                OnFocusChanged    (bool focused) override { SetFocused (focused); }
    std::wstring        GetAccessibleName () const override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::TabStrip; }

private:
    static constexpr int  s_kDragThresholdDip = 4;    // movement before a press becomes a drag
    static constexpr int  s_kDragScrollDip    = 16;   // scroll per move while a drag is held past an end
    static constexpr int  s_kWheelStepDip     = 60;   // scroll per wheel notch

    void  Commit         (int newIndex);
    bool  HasBounds      () const { return m_boundsDip.right > m_boundsDip.left; }
    int   GetMaxScrollPx () const;
    int   GetDropIndex   (int x) const;
    void  ClampScroll    ();
    void  ScrollIntoView (int index);
    void  MoveDraggedTab (int to);
    void  PaintInternal (IDxuiPainter & painter, IDxuiTextRenderer & text,
                         uint32_t idleArgb, uint32_t hoverArgb, uint32_t selectedArgb,
                         uint32_t textArgb, uint32_t focusArgb) const;


    std::vector<Tab>  m_tabs;
    ChangeFn          m_change;
    MoveFn            m_move;
    int               m_selected = 0;
    int               m_hover    = -1;
    int               m_pressed  = -1;
    int               m_pressX   = 0;
    int               m_scrollPx = 0;
    bool              m_dragging = false;
    bool              m_enabled  = true;
    bool              m_focused  = false;
    DxuiDpiScaler     m_scaler;
};
