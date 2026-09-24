#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Core/DxuiIconImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip
//
//  Horizontal tab selector. Owns an ordered list of (label, rect)
//  tabs and a single selected index. Mouse activates whichever tab
//  the click lands in; keyboard cycles Left / Right with wrap.
//
//  When the tabs do not fit, the strip shows a scroll arrow at each end, as
//  File Explorer does, and the tabs between them scroll into reach by the
//  arrows, the wheel, being selected, or a drag held past either end. A tab dragged along the
//  strip moves as it crosses its neighbors, and each move is reported.
//
//  With a new-tab handler set, a + button follows the last tab, or holds the
//  strip's right end when the tabs overflow.
//
//  Tabs are drawn in one of three styles:
//
//  - EXPLORER, as File Explorer draws them: an icon, a left-aligned label
//    and, with a close handler set, a close button, the selected tab filled
//    with the color of the row below so that it joins it.
//  - DOCUMENT, as Visual Studio draws its document tabs: compact, the
//    selected tab a rounded chip filled with the pane's color and outlined,
//    the others plain text, a close button on the selected tab and on the
//    one under the pointer only.
//  - TOOL WINDOW, as Visual Studio draws the tabs under a tool window: the
//    strip BELOW its pane, the selected tab the same rounded chip, the
//    others plain text.
//
//  In every style a tab can carry a leading mark ahead of its label, a glyph
//  in a face and color of the host's choosing, and a tip the host shows.
//
//  A tab dragged off the strip, past its top or bottom, is handed to the host
//  when it has asked for it, so a dock can take the tab away.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTabStrip : public IDxuiControl
{
public:
    using ChangeFn   = std::function<void (int newIndex)>;
    using MoveFn     = std::function<void (int from, int to)>;
    using NewTabFn   = std::function<void ()>;
    using CloseFn    = std::function<void (int index)>;
    using DragOutFn  = std::function<void (int index, POINT pointDip)>;

    enum class Style
    {
        Explorer,
        Document,
        ToolWindow,
    };

    DxuiTabStrip() { m_focusable = true; }

    struct Tab
    {
        RECT                                  rect     = {};
        std::wstring                          label;
        std::shared_ptr<const DxuiIconImage>  icon;   // drawn before the label; none draws none
        std::wstring                          tip;
        std::wstring                          mark;   // a glyph ahead of the label; empty draws none
        std::wstring                          markFace;   // the face it is drawn in; empty is the label's
        uint32_t                              markArgb = 0;
        bool                                  closable = true;   // with a close handler set, carries a close button
    };

    ~DxuiTabStrip() override = default;

    void  SetTabs    (std::vector<Tab> tabs);
    void  SetSelected (int index);
    void  SetEnabled (bool enabled) { IDxuiControl::SetEnabled (enabled); m_enabled = enabled; }
    void  SetFocused (bool focused) { m_focused = focused; }
    void  SetOnChange (ChangeFn fn) { m_change = std::move (fn); }
    void  SetOnMove   (MoveFn fn)   { m_move   = std::move (fn); }
    void  SetOnNewTab (NewTabFn fn) { m_newTab = std::move (fn); }

    //  With a close handler set, every closable tab carries a close button.
    void  SetOnClose  (CloseFn fn)  { m_close  = std::move (fn); }

    //  The color of the row the selected tab joins, which it is filled with,
    //  and the icon face its close glyph is drawn in.
    void  SetSelectedFill (uint32_t argb)        { m_selectedFill = argb; }
    void  SetIconFace     (const wchar_t * face) { m_iconFace     = face; }

    //  The strip's own fill, painted behind the tabs and cut from the selected
    //  tab's flared corners; none leaves the host's background showing.
    void  SetStripFill    (uint32_t argb)        { m_stripFill    = argb; }

    void  SetStyle        (Style style)          { m_style        = style; }
    Style GetStyle        () const               { return m_style; }

    //  The document and tool-window styles' outline round the selected tab:
    //  the accent in a focused group, a neutral color in another. Zero draws
    //  none.
    void  SetSelectedOutline (uint32_t argb)     { m_outlineArgb  = argb; }

    //  With a handler set, a tab dragged past the strip's top or bottom is
    //  handed to the host, and the strip lets go of it. With no move handler
    //  as well, the tabs keep their order and any drag is handed over.
    void  SetOnDragOut    (DragOutFn fn)         { m_dragOut      = std::move (fn); }

    //  The tip of the tab under a point, with the tab's rect, or empty.
    std::wstring  GetTipAt (int x, int y, RECT & tabRect) const;

    //  How wide a tab of this style is for its label, mark and icon, with or
    //  without a close button: what a host laying out the tabs gives it. A
    //  null renderer estimates the label from its length.
    static int  MeasureTabPx (IDxuiTextRenderer * text, const Tab & tab, Style style, bool hasClose, const DxuiDpiScaler & scaler);

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

    //  Where tab `index` is drawn, its strip rect moved by the scroll, and
    //  where the + button is; an empty rect when there is none.
    RECT  GetTabScreenRect (int index) const;
    RECT  GetNewTabRect    () const;

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
    static constexpr int  s_kIconInsetDip     = 10;   // icon from the tab's left edge
    static constexpr int  s_kIconDip          = 16;
    static constexpr int  s_kLabelInsetDip    = 38;   // label from the tab's left edge
    static constexpr int  s_kCloseCenterDip   = 22;   // close button's center from the tab's right edge
    static constexpr int  s_kCloseBoxDip      = 24;
    static constexpr int  s_kCloseGlyphDip    = 10;
    static constexpr int  s_kCornerDip        = 6;    // the selected tab's rounded and flared corners

    //  Visual Studio's document and tool-window tabs.
    static constexpr int  s_kCompactFontDip   = 12;
    static constexpr int  s_kCompactPadDip    = 8;    // label from each end of the tab
    static constexpr int  s_kCompactMarkDip   = 12;   // a leading mark's room
    static constexpr int  s_kCompactCloseDip  = 16;   // the close button's square
    static constexpr int  s_kCompactCornerDip = 4;
    static constexpr int  s_kCompactInsetDip  = 3;    // the selected chip from the strip's far edge
    static constexpr int  s_kCharEstimateDip  = 7;    // a label's width a character, unmeasured

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
    bool  IsOverNewTab   (int x, int y) const;
    void  PaintNewTab    (IDxuiPainter & painter, IDxuiTextRenderer & text, uint32_t hoverArgb, uint32_t textArgb) const;
    RECT  GetCloseRect   (int index) const;
    int   GetCloseAt     (int x, int y) const;
    void  PaintInternal (IDxuiPainter & painter, IDxuiTextRenderer & text,
                         uint32_t stripArgb, uint32_t hoverArgb, uint32_t fillArgb, uint32_t dividerArgb,
                         uint32_t textArgb, uint32_t focusArgb) const;
    void  PaintCompactTab (IDxuiPainter & painter, IDxuiTextRenderer & text, int index,
                           uint32_t hoverArgb, uint32_t fillArgb, uint32_t textArgb) const;
    bool  IsCloseShown  (int index) const;


    std::vector<Tab>  m_tabs;
    ChangeFn          m_change;
    MoveFn            m_move;
    int               m_selected      = 0;
    int               m_hover         = -1;
    int               m_pressed       = -1;
    int               m_pressX        = 0;
    int               m_pressY        = 0;
    Style             m_style         = Style::Explorer;
    uint32_t          m_outlineArgb   = 0;
    DragOutFn         m_dragOut;
    int               m_scrollPx      = 0;
    bool              m_dragging      = false;
    int               m_hoverArrow    = 0;   // -1 left, +1 right, 0 neither
    int               m_pressedArrow  = 0;
    NewTabFn          m_newTab;
    bool              m_hoverNewTab   = false;
    bool              m_pressedNewTab = false;
    CloseFn           m_close;
    int               m_hoverClose    = -1;
    int               m_pressedClose  = -1;
    uint32_t          m_selectedFill  = 0;
    uint32_t          m_stripFill     = 0;
    const wchar_t *   m_iconFace      = L"Segoe MDL2 Assets";
    bool              m_enabled       = true;
    bool              m_focused       = false;
    DxuiDpiScaler     m_scaler;
};
