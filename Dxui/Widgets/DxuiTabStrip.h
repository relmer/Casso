#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Core/DxuiIconImage.h"
#include "Core/DxuiPaneMetrics.h"





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
//    selected tab filled with the pane's color, rounded at its far corners
//    and open into the pane, the others plain text on the band, and the one
//    under the pointer in a faint box the band's full depth, rounded at its
//    far corners like the selected tab. The selected tab and the one under
//    the pointer show a pin and a close button; every tab keeps room for
//    both, so no tab changes width as they come and go.
//  - TOOL WINDOW, as Visual Studio draws the tabs under a tool window: the
//    same tabs, in a strip BELOW its pane.
//
//  In the compact styles the strip draws no outline: the pane's frame
//  (DxuiPaneFrame) runs around the selected tab and joins it to the pane.
//  Every label starts at the pane's text inset from its tab's left edge, so
//  the first tab's label lines up with a pane's title.
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
    using PinFn      = std::function<void (int index)>;
    using DragOutFn  = std::function<void (int index, POINT pointDip)>;

    enum class Style
    {
        Explorer,
        Document,
        ToolWindow,
    };

    //  A compact tab's own buttons.
    enum class TabButton
    {
        None,
        Pin,
        Close,
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

    //  With a pin handler set, a compact strip's selected tab and the one
    //  under the pointer show a pin, which docks the tab's pane or hides it
    //  against its edge.
    void  SetOnPin    (PinFn fn)    { m_pin    = std::move (fn); }

    //  The color of the row the selected tab joins, which it is filled with,
    //  and the icon face its close glyph is drawn in.
    void  SetSelectedFill (uint32_t argb)        { m_selectedFill = argb; }
    void  SetIconFace     (const wchar_t * face) { m_iconFace     = face; }

    //  The strip's own fill, painted behind the tabs; none leaves the host's
    //  background showing.
    void  SetStripFill    (uint32_t argb)        { m_stripFill    = argb; }

    void  SetStyle        (Style style)          { m_style        = style; }
    Style GetStyle        () const               { return m_style; }

    //  The look of the pane the user works in: its selected tab's label and
    //  glyphs in the full foreground, every other tab's a step dimmer.
    void  SetFocusedLook  (bool focused)         { m_focusedLook  = focused; }
    bool  HasFocusedLook  () const               { return m_focusedLook; }

    //  The outer radius of the pane's corners, which a pane too small to
    //  round has none of, and the tabs' rounded corners with it; -1, the
    //  default, is the pane metric's.
    void  SetCornerPx     (int px)               { m_cornerPx     = px; }

    //  With a handler set, a tab dragged past the strip's top or bottom is
    //  handed to the host, and the strip lets go of it. With no move handler
    //  as well, the tabs keep their order and any drag is handed over.
    void  SetOnDragOut    (DragOutFn fn)         { m_dragOut      = std::move (fn); }

    //  The tip of the tab under a point, with the tab's rect, or empty.
    std::wstring  GetTipAt (int x, int y, RECT & tabRect) const;

    //  How wide a tab of this style is for its label, mark and icon, with or
    //  without a close button: what a host laying out the tabs gives it. A
    //  compact tab keeps room for its pin and close button either way. A
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

    //  Where the last press of a tab fell, from that tab's top left.
    POINT  GetGrabOffset   () const { return m_grabOffset; }
    RECT  GetNewTabRect    () const;

    //  A gap opened ahead of tab `index` (the tab count for the end) where a
    //  dragged tab will land: the tabs from `index` on are drawn `widthPx`
    //  further right. An index of -1 closes it. GetInsertIndexAt is the gap a
    //  drop at `x` opens, by which half of a tab the point is over.
    void  SetInsertGap      (int index, int widthPx) { m_gapIndex = index; m_gapPx = widthPx; }
    int   GetInsertGapIndex () const                 { return m_gapIndex; }
    RECT  GetInsertGapRect  () const;
    int   GetInsertIndexAt  (int x) const;

    //  In the document and tool-window styles, where along the strip the
    //  selected tab shows, cut to the part between the scroll arrows, and
    //  whether an arrow cuts it off on either side, so the host can draw the
    //  pane's outline around it. False while no selected tab shows.
    bool  GetSelectedSpan  (long & left, long & right, bool & openLeft, bool & openRight) const;

    //  How far the wash under the pointer on a scroll arrow or the + stands
    //  in from the strip's edges.
    static int  GetHoverInsetPx (const DxuiDpiScaler & scaler);

    //  Where tab `index`'s pin or close button is in the compact styles,
    //  whether or not it shows: a square at the tab's end, between the tab's
    //  outline and the line along the band, the close button one line in
    //  from the tab's end and the pin beside it.
    RECT       GetTabButtonRect (int index, TabButton button) const;

    //  The shown pin or close button under a point, with its tab and its
    //  rect; None off every shown button.
    TabButton  GetTabButtonAt   (int x, int y, int & index, RECT & button) const;

    //  Visual Studio's inks for a pane's tabs and title: the full foreground
    //  for the selected tab of the pane the user works in, and for every
    //  other label a step toward the muted foreground, kept readable on the
    //  band; the pin and close glyphs a shade under their label. A hovered
    //  tab is the foreground laid faintly over the band.
    static uint32_t  GetLabelInk     (const IDxuiTheme & theme, bool focusedSelected);
    static uint32_t  GetGlyphInk     (const IDxuiTheme & theme, bool focusedSelected);
    static uint32_t  GetTabHoverFill (const IDxuiTheme & theme);

    //  A tab's pin and close squares, as big as a pane's title bar buttons,
    //  and their glyphs in Segoe MDL2 Assets, sized so their ink is 16 px
    //  square at 150%, as Visual Studio's is.
    static constexpr int    kTabButtonDip  = 24;
    static constexpr float  kCloseGlyphDip = 14.0f;
    static constexpr float  kPinGlyphDip   = 10.67f;

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
    static constexpr int  s_kCompactMarkDip   = 12;   // a leading mark's room
    static constexpr int  s_kCompactCornerDip = 4;    // a wash's rounded corners
    static constexpr int  s_kCompactInsetDip  = 3;    // an arrow's or the +'s wash from the strip's edges
    static constexpr int  s_kCharEstimateDip  = 7;    // a label's width a character, unmeasured

    struct Palette;

    void  Commit            (int newIndex);
    bool  HasBounds         () const { return m_boundsDip.right > m_boundsDip.left; }
    int   GetMaxScrollPx    () const;
    int   GetDropIndex      (int x) const;
    void  ClampScroll       ();
    void  ScrollIntoView    (int index);
    void  MoveDraggedTab    (int to);
    bool  IsOverflowing     () const;
    int   GetArrowWidthPx   () const;
    int   GetViewLeft       () const { return (int) m_boundsDip.left  + GetArrowWidthPx(); }
    int   GetViewRight      () const { return (int) m_boundsDip.right - GetNewTabWidthPx() - GetArrowWidthPx(); }
    int   GetArrowAt        (int x, int y) const;
    bool  CanScroll         (int direction) const;
    void  ScrollByTab       (int direction);
    void  PaintArrow        (IDxuiPainter & painter, IDxuiTextRenderer & text, int direction, uint32_t hoverArgb, uint32_t textArgb) const;
    int   GetNewTabWidthPx  () const;
    bool  IsOverNewTab      (int x, int y) const;
    void  PaintNewTab       (IDxuiPainter & painter, IDxuiTextRenderer & text, uint32_t hoverArgb, uint32_t textArgb) const;
    RECT  GetCloseRect      (int index) const;
    int   GetCloseAt        (int x, int y) const;
    int   GetPinAt          (int x, int y) const;
    int   GetCornerPx       () const;
    void  PaintInternal     (IDxuiPainter & painter, IDxuiTextRenderer & text, const Palette & pal) const;
    void  PaintCompactTab   (IDxuiPainter & painter, IDxuiTextRenderer & text, int index, const Palette & pal) const;
    void  PaintTabButton    (IDxuiPainter & painter, IDxuiTextRenderer & text, int index, TabButton button, const Palette & pal) const;
    void  PaintSelectedBody (IDxuiPainter & painter, const RECT & tab, uint32_t fillArgb) const;
    void  PaintHoverBox     (IDxuiPainter & painter, const RECT & tab, uint32_t argb) const;
    void  PaintHoverPill    (IDxuiPainter & painter, const RECT & rect, uint32_t argb) const;
    bool  IsCloseShown      (int index) const;
    bool  IsPinShown        (int index) const;


    std::vector<Tab>  m_tabs;
    ChangeFn          m_change;
    MoveFn            m_move;
    int               m_selected      = 0;
    int               m_hover         = -1;
    int               m_pressed       = -1;
    int               m_pressX        = 0;
    int               m_pressY        = 0;
    POINT             m_grabOffset    = {};
    Style             m_style         = Style::Explorer;
    DragOutFn         m_dragOut;
    int               m_scrollPx      = 0;
    bool              m_dragging      = false;
    int               m_hoverArrow    = 0;   // -1 left, +1 right, 0 neither
    int               m_pressedArrow  = 0;
    NewTabFn          m_newTab;
    bool              m_hoverNewTab   = false;
    bool              m_pressedNewTab = false;
    CloseFn           m_close;
    PinFn             m_pin;
    int               m_hoverClose    = -1;
    int               m_pressedClose  = -1;
    int               m_hoverPin      = -1;
    int               m_pressedPin    = -1;
    uint32_t          m_selectedFill  = 0;
    uint32_t          m_stripFill     = 0;
    const wchar_t *   m_iconFace      = L"Segoe MDL2 Assets";
    bool              m_enabled       = true;
    bool              m_focused       = false;
    bool              m_focusedLook   = false;
    int               m_cornerPx      = -1;
    int               m_gapIndex      = -1;
    int               m_gapPx         = 0;
    DxuiDpiScaler     m_scaler;
};
