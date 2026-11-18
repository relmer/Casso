#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Core/DxuiPaneMetrics.h"
#include "Widgets/DxuiTabSlides.h"
#include "Widgets/DxuiTabStripPainter.h"
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
//  Document  Visual Studio's document tabs: compact, the selected tab filled
//            with the pane's color, rounded at its far corners and open into
//            the pane, the others plain text on the band, and the one under
//            the pointer in a faint box the band's full depth. The selected
//            tab and the one under the pointer show a pin and a close button;
//            every tab keeps room for both, so no tab changes width as they
//            come and go.
//
//  ToolWindow  The tabs under a Visual Studio tool window: the same tabs,
//            with no pin or close button, in a strip BELOW its pane.
//
//  In the compact styles the strip draws no outline: the pane's frame
//  (DxuiPaneFrame) runs around the selected tab and joins it to the pane.
//  Every label starts at the pane's text inset from its tab's left edge, so
//  the first tab's label lines up with a pane's title.
//
////////////////////////////////////////////////////////////////////////////////

enum class DxuiTabStripStyle
{
    Standard,
    Explorer,
    Document,
    ToolWindow,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStrip
//
//  Horizontal tab selector. Owns an ordered list of (label, rect)
//  tabs and a single selected index. Mouse activates whichever tab
//  the click lands in; keyboard cycles Left / Right with wrap.
//
//  The style (DxuiTabStripStyle) sets how the tabs are drawn. When the tabs do
//  not fit, a strip in either style has a scroll arrow at each end, as File
//  Explorer's does. The tabs between them scroll into view with the arrows,
//  the wheel, a selection, or a drag past either end.
//
//  The rest is the host's to turn on, in either style. With a move handler
//  set, a tab can be dragged, as File Explorer's are: the dragged tab is
//  lifted out of the row, the others close up and slide apart to open a gap
//  where it would drop, and the + button slides along after the last of
//  them. The move is reported once, on the drop. Dragged far enough off the
//  strip, the tab is torn off: the gap closes, and the host handles the drop.
//  Without a move handler, a press is canceled once the pointer moves off
//  its tab. A host can also open a gap for a tab dragged in from elsewhere
//  (SetIncomingDrag). With a new-tab handler set, a + button follows the
//  last tab, or holds the strip's right end when the tabs overflow. With a close handler
//  set, every tab gets a close button. A tab with an icon draws it before its
//  label.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTabStrip : public IDxuiControl
{
public:
    //  Where the new-tab button is, in the strip's coordinates.
    RECT  GetNewTabRect    () const;

    using  ChangeFn  = std::function<void (int newIndex)>;
    using  MoveFn    = std::function<void (int from, int to)>;
    using  NewTabFn  = std::function<void ()>;
    using  CloseFn   = std::function<void (int index)>;
    using  DragFn    = std::function<void (int index, POINT point, bool tornOff)>;
    using  ClockFn   = std::function<int64_t ()>;
    using  PinFn     = std::function<void (int index)>;
    using  DragOutFn = std::function<void (int index, POINT pointDip)>;
    using  Style     = DxuiTabStripStyle;

    //  A compact tab's own buttons.
    enum class TabButton
    {
        None,
        Pin,
        Close,
    };

    // Label metrics: the size a tab label is drawn at, and the inset the
    // label keeps from its tab's sides (in the Explorer style, from its right
    // side only). Callers sizing tabs to their labels measure at the same
    // size, in GetLabelFace's face, at normal weight.
    static constexpr float  kLabelFontDip         = 13.0f;
    static constexpr float  kExplorerLabelFontDip = 12.0f;   // File Explorer's, measured at 150%
    static constexpr float  kLabelPadXDip         = 8.0f;

    static const wchar_t *  GetLabelFace();

    DxuiTabStrip();

    struct Tab
    {
        RECT                                  rect     = {};
        std::wstring                          label;
        std::shared_ptr<const DxuiIconImage>  icon;   // drawn before the label; none draws none
        std::wstring                          tip;
        std::wstring                          mark;   // a glyph ahead of the label; empty draws none
        std::wstring                          markFace;   // the face it is drawn in; empty is the system face
        uint32_t                              markArgb = 0;
        bool                                  closable = true;   // with a close handler set, has a close button
    };

    ~DxuiTabStrip() override = default;

    void  SetTabs     (std::vector<Tab> tabs);
    void  SetTabIcon  (size_t index, std::shared_ptr<const DxuiIconImage> icon) { m_tabs[index].icon = std::move (icon); }
    void  SetSelected (int index);
    void  SetEnabled  (bool enabled) { IDxuiControl::SetEnabled (enabled); m_enabled = enabled; }
    void  SetFocused  (bool focused) { m_focused = focused; }
    void  SetStyle    (DxuiTabStripStyle style) { m_style = style; }
    void  SetOnChange (ChangeFn fn) { m_change = std::move (fn); }

    //  With a move handler set, a tab can be dragged to a new place.
    void  SetOnMove   (MoveFn fn)   { m_move   = std::move (fn); }
    void  SetOnNewTab (NewTabFn fn) { m_newTab = std::move (fn); }

    //  Room between the last tab and the + button.
    void  SetNewTabGapDip (float dip) { m_newTabGapDip = dip; }

    //  With a close handler set, every closable tab gets a close button, in
    //  every style but the tool window's.
    void  SetOnClose  (CloseFn fn)  { m_close  = std::move (fn); }

    //  With a pin handler set, a document strip's selected tab and the one
    //  under the pointer show a pin, which docks the tab's pane or hides it
    //  against its edge.
    void  SetOnPin    (PinFn fn)    { m_pin    = std::move (fn); }

    //  The look of the pane the user works in: its selected tab's label and
    //  glyphs in the full foreground, every other tab's a step dimmer.
    void  SetFocusedLook  (bool focused)         { m_focusedLook  = focused; }
    bool  HasFocusedLook  () const               { return m_focusedLook; }

    //  The outer radius of the pane's corners, which a pane too small to
    //  round has none of, and the tabs' rounded corners with it; -1, the
    //  default, is the pane metric's.
    void  SetCornerPx     (int px)               { m_cornerPx     = px; }

    //  With a handler set, a tab dragged past the strip's top or bottom, or
    //  past either end of its row of tabs, is given to the host, and the
    //  strip lets go of it. With no move handler as well, the tabs keep their
    //  order and any drag goes to the host.
    void  SetOnDragOut    (DragOutFn fn)         { m_dragOut      = std::move (fn); }

    //  The tip of the tab under a point, with the tab's rect, or empty.
    std::wstring  GetTipAt (int x, int y, RECT & tabRect) const;

    //  How wide a tab of this style is for its label, mark and icon, with or
    //  without a close button: what a host laying out the tabs gives it. A
    //  document tab keeps room for its pin and close button either way. A
    //  null renderer estimates the label from its length.
    static int  MeasureTabPx (IDxuiTextRenderer * text, const Tab & tab, Style style, bool hasClose, const DxuiDpiScaler & scaler);

    //  Where the last press of a tab fell, from that tab's top left.
    POINT  GetGrabOffset   () const { return m_grab; }

    //  A gap opened ahead of tab `index` (the tab count for the end) where a
    //  dragged pane will land as a tab: the tabs from `index` on are drawn
    //  `widthPx` further right. An index of -1 closes it. GetInsertIndexAt is
    //  the gap a drop at `x` opens, by which half of a tab the point is over.
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

    //  Where tab `index`'s pin or close button is in the document style,
    //  whether or not it shows: a square at the tab's end, between the tab's
    //  outline and the line along the band, the close button one line in
    //  from the tab's end and the pin beside it. An empty rect in the other
    //  styles.
    RECT       GetTabButtonRect (int index, TabButton button) const;

    //  The shown pin or close button under a point, with its tab and its
    //  rect; None off every shown button.
    TabButton  GetTabButtonAt   (int x, int y, int & index, RECT & button) const;

    //  Visual Studio's inks for a pane's tabs and title: the full foreground
    //  for the selected tab of the pane the user works in, and for every
    //  other label a step toward the muted foreground, kept readable on the
    //  band; the label of the tab under the pointer a little nearer the
    //  foreground; the pin and close glyphs a shade under their label. A
    //  hovered tab is the foreground laid faintly over the band.
    static uint32_t  GetLabelInk        (const IDxuiTheme & theme, bool focusedSelected);
    static uint32_t  GetGlyphInk        (const IDxuiTheme & theme, bool focusedSelected);
    static uint32_t  GetHoveredLabelInk (const IDxuiTheme & theme);
    static uint32_t  GetHoveredGlyphInk (const IDxuiTheme & theme);
    static uint32_t  GetTabHoverFill    (const IDxuiTheme & theme);

    //  A tab's pin and close squares, as big as a pane's title bar buttons,
    //  and their glyphs in Segoe MDL2 Assets, sized so their ink is 16 px
    //  square at 150%, as Visual Studio's is.
    static constexpr int    kTabButtonDip  = 24;
    static constexpr float  kCloseGlyphDip = 14.0f;
    static constexpr float  kPinGlyphDip   = 10.67f;

    //  A drag's progress, on every move of the pointer, and its drop. The
    //  point is in the strip's own coordinates; `tornOff` is set while the
    //  pointer is far enough off the strip that a drop tears the tab off. A
    //  drop that was not torn off has already moved the tab, and its index
    //  is where it landed.
    void  SetOnDragMove (DragFn fn) { m_dragMove = std::move (fn); }
    void  SetOnDragEnd  (DragFn fn) { m_dragEnd  = std::move (fn); }

    //  A tab dragged in from elsewhere, `widthPx` wide with the pointer at its
    //  center: a gap opens where it would drop, until it is cleared.
    //  GetIncomingIndex is where the tab would go, or -1 with none.
    void  SetIncomingDrag   (int x, int widthPx) { m_dragSlides.SetIncomingDrag (x, widthPx); }
    void  ClearIncomingDrag () { m_dragSlides.ClearIncomingDrag(); }
    int   GetIncomingIndex  () const { return m_dragSlides.GetIncomingIndex(); }

    //  The time slides are measured on, in milliseconds; GetTickCount64 by
    //  default. Slides run only while animations are enabled, which follows
    //  the system's setting unless set.
    void  SetClock             (ClockFn fn) { m_dragSlides.SetClock (std::move (fn)); }
    void  SetAnimationsEnabled (bool on)    { m_dragSlides.SetAnimationsEnabled (on); }

    //  True while any tab or the + button is sliding to its place; the host
    //  repaints until it is false.
    bool  IsSliding () const { return m_dragSlides.IsSliding(); }

    //  Where tab `index` is drawn now, sliding included, in the strip's own
    //  coordinates.
    RECT  GetTabScreenRect (int index) const;

    //  Ends a drag with nothing moved and no drop reported, as Escape does:
    //  the gap closes and the tab slides back to its place.
    void  CancelDrag       ();

    bool  IsDragging       () const { return m_dragging; }
    bool  IsTornOff        () const { return m_dragging && m_tornOff; }

    //  Where in the dragged tab the pointer took it, from its top-left corner.
    POINT GetDragGrab      () const { return m_grab; }

    //  Draws tab `index` as a dragged tab's card, `widthPx` by `heightPx` from
    //  the origin: its icon and its label, drawn as on the selected tab.
    void  PaintFloatingTab (IDxuiTextRenderer & text, int index, float widthPx, float heightPx, uint32_t textArgb) const { m_painter.PaintFloatingTab (text, index, widthPx, heightPx, textArgb); }

    //  The Explorer style's selected tab fill, the color of the row it joins,
    //  and the icon face the close glyph is drawn in.
    void  SetSelectedFill (uint32_t argb)        { m_painter.SetSelectedFill (argb); }
    void  SetIconFace     (const wchar_t * face) { m_painter.SetIconFace (face); }

    //  The Explorer style's own strip fill, painted behind the tabs; none
    //  leaves the host's background showing.
    void  SetStripFill    (uint32_t argb)        { m_painter.SetStripFill (argb); }

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
    static constexpr int  s_kTearOffDip       = 24;   // distance off the strip that tears a tab off
    static constexpr int  s_kSlideMs          = 160;  // a tab's slide to its new place
    static constexpr int  s_kWheelStepDip     = 60;   // scroll per wheel notch
    static constexpr int  s_kArrowWidthDip    = 28;   // each scroll arrow

    //  File Explorer's tab, measured at 120 dpi.
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

    //  Visual Studio's document and tool-window tabs.
    static constexpr int    s_kCompactFontDip   = 12;
    static constexpr int    s_kCompactMarkDip   = 12;   // a leading mark's room
    static constexpr int    s_kCompactCornerDip = 4;    // a wash's rounded corners
    static constexpr int    s_kCompactInsetDip  = 3;    // an arrow's or the +'s wash from the strip's edges
    static constexpr int    s_kCharEstimateDip  = 7;    // a label's width a character, unmeasured

    //  A pressed tab's, button's, arrow's or +'s wash, a touch darker than
    //  under the pointer; and a tab's pin and close glyphs against its label,
    //  Visual Studio's #D1D1D1 beside #D7D7D7.
    static constexpr float  kPressedScale = 0.82f;
    static constexpr float  kGlyphScale   = 0.973f;

    bool  IsCompact     () const { return m_style == Style::Document || m_style == Style::ToolWindow; }
    bool  IsOffTheTabs  (int x, int y, int threshold) const;
    int   GetPinAt      (int x, int y) const;
    int   GetCornerPx   () const;
    bool  IsCloseShown  (int index) const;
    bool  IsPinShown    (int index) const;

    void  Commit           (int newIndex);
    bool  HasBounds        () const { return m_boundsDip.right > m_boundsDip.left; }
    int   GetMaxScrollPx   () const;
    int   GetContentX      (int x) const { return x + m_scrollPx - GetArrowWidthPx(); }
    void  EndDrag          (int x, int y);
    void  ClampScroll      ();
    void  ScrollIntoView   (int index);
    void  MoveDraggedTab   (int to);
    bool  IsOverflowing    () const;
    int   GetArrowWidthPx  () const;
    int   GetViewLeft      () const { return (int) m_boundsDip.left  + GetArrowWidthPx(); }
    int   GetViewRight     () const { return (int) m_boundsDip.right - GetNewTabWidthPx() - GetArrowWidthPx(); }
    int   GetArrowAt       (int x, int y) const;
    bool  CanScroll        (int direction) const;
    void  ScrollByTab      (int direction);
    int   GetNewTabWidthPx () const;
    bool  IsOverNewTab     (int x, int y) const;
    RECT  GetCloseRect     (int index) const;

    //  The tab drawn as selected: none while the selected tab is the one being
    //  dragged, which is not drawn in the row. And whether tab `index` is
    //  drawn at all.
    int   GetPaintedSelection () const;
    bool  IsPainted           (int index) const { return !(m_dragging && index == m_pressed); }

    int       GetCloseAt          (int x, int y) const;


    std::vector<Tab>   m_tabs;
    DxuiTabStripStyle  m_style         = DxuiTabStripStyle::Standard;
    ChangeFn           m_change;
    MoveFn             m_move;
    int                m_selected      = 0;
    int                m_hover         = -1;
    int                m_pressed       = -1;
    int                m_pressX        = 0;
    int                m_pressY        = 0;
    int                m_scrollPx      = 0;
    bool               m_dragging      = false;
    bool               m_tornOff       = false;
    POINT              m_grab          = {};
    int                m_dragX         = 0;
    DragFn             m_dragMove;
    DragFn             m_dragEnd;
    int                m_hoverArrow    = 0;   // -1 left, +1 right, 0 neither
    int                m_pressedArrow  = 0;
    NewTabFn           m_newTab;
    float              m_newTabGapDip  = 0.0f;
    bool               m_hoverNewTab   = false;
    bool               m_pressedNewTab = false;
    CloseFn            m_close;
    int                m_hoverClose    = -1;
    int                m_pressedClose  = -1;
    PinFn              m_pin;
    int                m_hoverPin      = -1;
    int                m_pressedPin    = -1;
    DragOutFn          m_dragOut;
    bool               m_focusedLook   = false;
    int                m_cornerPx      = -1;
    int                m_gapIndex      = -1;
    int                m_gapPx         = 0;
    bool               m_enabled       = true;
    bool               m_focused       = false;
    DxuiDpiScaler      m_scaler;

    friend class DxuiTabSlides;
    friend class DxuiTabStripPainter;

    DxuiTabSlides        m_dragSlides { *this };
    DxuiTabStripPainter  m_painter    { *this };
};
