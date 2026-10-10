#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Core/DxuiIconImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStripStyle
//
//  How a DxuiTabStrip draws its tabs.
//
//  Standard  the Dxui dialog look, and the default: a centered label, the
//            selected tab marked by an accent underline, the rest unfilled
//            with dimmed labels.
//
//  Explorer  File Explorer's tab row: an icon and a left-aligned label, the
//            selected tab filled with the color of the row below so that it
//            joins it, short dividers between the rest, and scroll arrows
//            when the tabs do not fit.
//
////////////////////////////////////////////////////////////////////////////////

enum class DxuiTabStripStyle
{
    Standard,
    Explorer,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip
//
//  Horizontal tab selector. Owns an ordered list of (label, rect)
//  tabs and a single selected index. Mouse activates whichever tab
//  the click lands in; keyboard cycles Left / Right with wrap.
//
//  The style (DxuiTabStripStyle) sets how the tabs are drawn, and whether the
//  strip scrolls. Only the Explorer style does: when its tabs do not fit, it
//  shows a scroll arrow at each end, as File Explorer does, and the tabs
//  between them scroll into reach by the arrows, the wheel, being selected,
//  or a drag held past either end. A Standard strip draws tabs that do not
//  fit past its end.
//
//  The rest is the host's to turn on, in either style. With a move handler
//  set, a tab dragged along the strip moves as it crosses its neighbors, and
//  each move is reported; without one, a press that leaves its tab is
//  canceled. With a new-tab handler set, a + button follows the last tab, or
//  holds the strip's right end when the tabs overflow. With a close handler
//  set, every tab gets a close button. A tab with an icon draws it before its
//  label.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTabStrip : public IDxuiControl
{
public:
    using ChangeFn = std::function<void (int newIndex)>;
    using MoveFn   = std::function<void (int from, int to)>;
    using NewTabFn = std::function<void ()>;
    using CloseFn  = std::function<void (int index)>;

    // Label metrics: the size a tab label is drawn at, and the inset the
    // label keeps from its tab's sides (in the Explorer style, from its right
    // side only). Callers sizing tabs to their labels measure at the same
    // size, in GetLabelFace's face, at normal weight.
    static constexpr float  kLabelFontDip         = 13.0f;
    static constexpr float  kExplorerLabelFontDip = 12.0f;   // File Explorer's, measured at 150%
    static constexpr float  kLabelPadXDip         = 8.0f;

    static const wchar_t *  GetLabelFace();

    DxuiTabStrip() { m_focusable = true; }

    struct Tab
    {
        RECT                                  rect = {};
        std::wstring                          label;
        std::shared_ptr<const DxuiIconImage>  icon;    // drawn before the label; none draws none
    };

    ~DxuiTabStrip() override = default;

    void  SetTabs    (std::vector<Tab> tabs);
    void  SetSelected (int index);
    void  SetEnabled (bool enabled) { IDxuiControl::SetEnabled (enabled); m_enabled = enabled; }
    void  SetFocused (bool focused) { m_focused = focused; }
    void  SetStyle    (DxuiTabStripStyle style) { m_style = style; }
    void  SetOnChange (ChangeFn fn) { m_change = std::move (fn); }

    //  With a move handler set, a tab can be dragged to a new place.
    void  SetOnMove   (MoveFn fn)   { m_move   = std::move (fn); }
    void  SetOnNewTab (NewTabFn fn) { m_newTab = std::move (fn); }

    //  Room between the last tab and the + button.
    void  SetNewTabGapDip (float dip) { m_newTabGapDip = dip; }

    //  With a close handler set, every tab gets a close button.
    void  SetOnClose  (CloseFn fn)  { m_close  = std::move (fn); }

    //  The Explorer style's selected tab fill, the color of the row it joins,
    //  and the icon face the close glyph is drawn in.
    void  SetSelectedFill (uint32_t argb)        { m_selectedFill = argb; }
    void  SetIconFace     (const wchar_t * face) { m_iconFace     = face; }

    //  The Explorer style's own strip fill, painted behind the tabs; none
    //  leaves the host's background showing.
    void  SetStripFill    (uint32_t argb)        { m_stripFill    = argb; }

    DxuiTabStripStyle        GetStyle      () const { return m_style;   }
    const std::vector<Tab> & GetTabs       () const { return m_tabs;    }
    int                      GetSelected   () const { return m_selected; }
    int                      GetHoverIndex () const { return m_hover;   }
    bool                     IsEnabled     () const { return m_enabled; }
    bool                     IsFocused     () const { return m_focused; }
    int                      GetScrollPx   () const { return m_scrollPx; }

    //  True from a press on a tab until the button comes up, so the host can
    //  keep sending moves to a drag that has left the strip.
    bool                     IsInteracting () const { return m_pressed >= 0; }

    //  True while the tabs are wider than the strip and the arrows are shown.
    bool                     HasScrollArrows () const { return IsOverflowing(); }

    //  The + button's width, which a host sizing its tabs leaves free.
    static constexpr int     kNewTabWidthDip = 32;

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
    static constexpr int  s_kArrowWidthDip    = 28;   // each scroll arrow

    //  File Explorer's tab, measured at 120 dpi (research R13).
    static constexpr int  s_kIconInsetDip     = 8;    // icon from the tab's left edge
    static constexpr int  s_kIconDip          = 16;
    static constexpr int  s_kLabelInsetDip    = 34;   // label from the tab's left edge
    static constexpr int  s_kCloseCenterDip   = 22;   // close button's center from the tab's right edge
    static constexpr int  s_kCloseBoxDip      = 24;
    static constexpr int  s_kCloseGlyphDip    = 8;

    //  File Explorer's, measured at 125 and 150%.
    static constexpr float  s_kExplorerCloseCenterDip = 20.67f;
    static constexpr float  s_kExplorerCloseGlyphDip  = 8.67f;
    static constexpr int    s_kCornerDip              = 6;   // the selected tab's rounded top corners

    void  Commit         (int newIndex);
    bool  HasBounds      () const { return m_boundsDip.right > m_boundsDip.left; }
    int   GetMaxScrollPx () const;
    int   GetDropIndex   (int x) const;
    void  ClampScroll    ();
    void  ScrollIntoView (int index);
    void  MoveDraggedTab (int to);
    bool  IsOverflowing  () const;
    int   GetArrowWidthPx () const;
    int   GetViewLeft    () const { return (int) m_boundsDip.left  + GetArrowWidthPx(); }
    int   GetViewRight   () const { return (int) m_boundsDip.right - GetNewTabWidthPx() - GetArrowWidthPx(); }
    int   GetArrowAt     (int x, int y) const;
    bool  CanScroll      (int direction) const;
    void  ScrollByTab    (int direction);
    void  PaintArrow     (IDxuiPainter & painter, IDxuiTextRenderer & text, int direction, uint32_t hoverArgb, uint32_t textArgb) const;
    int   GetNewTabWidthPx () const;
    RECT  GetNewTabRect  () const;
    bool  IsOverNewTab   (int x, int y) const;
    void  PaintNewTab    (IDxuiPainter & painter, IDxuiTextRenderer & text, uint32_t hoverArgb, uint32_t textArgb) const;
    RECT  GetTabScreenRect (int index) const;
    RECT  GetCloseRect   (int index) const;

    //  A tab's hover fill, darker while pressed; and what a tab shows: the
    //  strip, or its hover mixed onto the strip.
    uint32_t  GetHoverFill        (int index, uint32_t hoverArgb) const;
    uint32_t  GetBesideFill       (int index, uint32_t stripArgb, uint32_t hoverArgb) const;
    int       GetCloseAt          (int x, int y) const;
    bool      HasTabIcon          (int index) const;
    void      PaintTabIcon        (IDxuiTextRenderer & text, int index) const;
    void      PaintTabClose       (IDxuiPainter & painter, IDxuiTextRenderer & text, int index, uint32_t glyphArgb, uint32_t washArgb, float cornerPx) const;
    void      PaintStandard       (IDxuiPainter & painter, IDxuiTextRenderer & text,
                                   uint32_t hoverArgb, uint32_t underlineArgb,
                                   uint32_t textArgb, uint32_t focusArgb) const;
    void      PaintExplorer       (IDxuiPainter & painter, IDxuiTextRenderer & text,
                                   uint32_t stripArgb, uint32_t hoverArgb, uint32_t fillArgb, uint32_t dividerArgb,
                                   uint32_t textArgb, uint32_t focusArgb) const;
    void      PaintExplorerThemed (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const;


    std::vector<Tab>   m_tabs;
    DxuiTabStripStyle  m_style         = DxuiTabStripStyle::Standard;
    ChangeFn           m_change;
    MoveFn             m_move;
    int                m_selected      = 0;
    int                m_hover         = -1;
    int                m_pressed       = -1;
    int                m_pressX        = 0;
    int                m_scrollPx      = 0;
    bool               m_dragging      = false;
    int                m_hoverArrow    = 0;   // -1 left, +1 right, 0 neither
    int                m_pressedArrow  = 0;
    NewTabFn           m_newTab;
    float              m_newTabGapDip  = 0.0f;
    bool               m_hoverNewTab   = false;
    bool               m_pressedNewTab = false;
    CloseFn            m_close;
    int                m_hoverClose    = -1;
    int                m_pressedClose  = -1;
    uint32_t           m_selectedFill  = 0;
    uint32_t           m_stripFill     = 0;
    const wchar_t *    m_iconFace      = L"Segoe MDL2 Assets";
    bool               m_enabled       = true;
    bool               m_focused       = false;
    DxuiDpiScaler      m_scaler;
};
