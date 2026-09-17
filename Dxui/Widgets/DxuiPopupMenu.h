#pragma once

#include "Pch.h"
#include "Core/DxuiCommand.h"
#include "Theme/DxuiMenuMetrics.h"
#include "Core/IDxuiControl.h"


class DxuiHwndSource;
class DxuiPopupHost;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuItem
//
//  One row of a popup menu: a command, a separator, or a command whose row
//  opens a child list. The item shares ownership of its command, so placing
//  one command in several menus never copies the declaration, and a menu on
//  screen keeps its rows alive however the application rebuilds its own
//  lists in the meantime.
//
//  A row is CHECKABLE when its command supplies an `isChecked` functor, and
//  a list containing any checkable row reserves a check gutter for every row.
//  A list where nothing can check reserves none and its labels sit flush.
//
////////////////////////////////////////////////////////////////////////////////



struct DxuiPopupMenuItem
{
    enum class Kind
    {
        Command,
        Separator,
        Submenu,

        //  A row of icon buttons, one per child command, as Explorer puts Cut,
        //  Copy, Paste, Rename and Delete along the edge of its menu nearest
        //  the pointer. It stays first in the list the caller builds and moves
        //  to the end, with the separator after it, when the menu opens
        //  upward. The keyboard passes over it; its commands are elsewhere.
        IconRow,
    };

    Kind                                kind     = Kind::Command;
    std::shared_ptr<const DxuiCommand>  command;
    std::vector<DxuiPopupMenuItem>      children;

    static DxuiPopupMenuItem  ForCommand   (std::shared_ptr<const DxuiCommand> cmd);
    static DxuiPopupMenuItem  ForSeparator ();
    static DxuiPopupMenuItem  ForSubmenu   (std::shared_ptr<const DxuiCommand> cmd, std::vector<DxuiPopupMenuItem> children);
    static DxuiPopupMenuItem  ForIconRow   (std::vector<std::shared_ptr<const DxuiCommand>> commands);
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenu
//
//  The one menu, whatever opens it. A menu bar title hangs it under a rect,
//  a toolbar drop-down button hangs it under the button, and a right-click
//  raises it at a point; the widget never learns which.
//
//  Rows come from `DxuiPopupMenuItem` and every label, check, accelerator and
//  enabled state is read from the row's command AT PAINT TIME, never cached.
//
//  Row height, font and gutter come from `DxuiMenuMetrics`, which reads the
//  Windows menu settings, so the menu is the size a real menu is on the same
//  display at the same font. Width fits the content: the label column is the
//  widest label and the accelerator column the widest accelerator, and the
//  two are separate columns, so accelerators line up on their left edges and
//  no label can reach into them. There is no fixed width and no setter for
//  one, only a floor.
//
//  Up and Down skip separators and disabled rows and wrap. Right on a submenu
//  row opens its child with the first enabled row highlighted; the pointer
//  resting on it opens the child unhighlighted, and hovering another row of
//  the parent closes it again. Left or Escape with a child open closes only
//  the child. Escape on the root hides it uncommitted.
//
//  Three callbacks. Highlight change fires on every move by pointer or key,
//  for a caller that previews. Closed fires once per visible-to-hidden edge,
//  BEFORE select, with a flag saying whether a row was picked. Select fires
//  with the picked row's index, and then the row's command dispatches if it
//  is enabled. For a pick inside a submenu the index is the row within that
//  submenu; the command is the reliable handle.
//
//  With a popup host each level acquires a pooled top-level popup on show
//  and releases it on hide, and a child links to its parent through the host
//  so click-outside dismisses the whole chain. Without a host the owner
//  paints the menu and routes input to it.
//
//  A show requested within a short window of the last hide, from the same
//  anchor, is ignored. A click on the title or button that opened the menu
//  reaches the strip after the popup has already dismissed itself on that
//  same click, and without the guard the release would open the menu again
//  and the button would never appear to toggle.
//
//  The legacy `Item` list and the show that takes it remain for the callers
//  that still build rows as a label and a checked flag. They are converted
//  to commands the widget owns, so there is one paint path and one
//  navigation path. Both go once the last such caller has moved.
//
//  Every public method runs on the UI thread.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiPopupMenu : public IDxuiControl
{
public:
    struct Item
    {
        std::wstring  label;
        bool          checked = false;
    };

    using SelectFn = std::function<void (int index)>;
    using ClosedFn = std::function<void (bool committed)>;
    using ClockFn  = std::function<uint64_t ()>;

    DxuiPopupMenu  ();
    ~DxuiPopupMenu () override;

    void  SetDpi      (UINT dpi)                { m_scaler.SetDpi (dpi); RefreshMetrics(); }
    void  SetTheme    (const IDxuiTheme * th)   { m_theme = th; }
    void  SetOnSelect (SelectFn fn)             { m_onSelect = std::move (fn); }

    void  SetOnHighlightChange (SelectFn fn)    { m_onHighlight = std::move (fn); }
    void  SetOnClosed          (ClosedFn fn)    { m_onClosed = std::move (fn); }

    // Where a click that dismissed this menu landed, in screen pixels. See
    // DxuiPopupHost::Params::onClickOutside.
    void  SetOnClickOutside (std::function<void (POINT screenPx)> fn)
              { m_onClickOutside = std::move (fn); }

    //  Colors an application supplies in place of the theme's, so a host
    //  whose chrome palette differs from the generic mapping lands its
    //  override in the one place every menu paints from. Disabled text always
    //  comes from the theme.
    void  SetColors   (uint32_t bgArgb,
                       uint32_t hoverArgb,
                       uint32_t textArgb,
                       uint32_t accelArgb,
                       uint32_t borderArgb,
                       uint32_t dividerArgb);
    void  ClearColors ()                        { m_colorsSet = false; }

    //  Underline each row's mnemonic letter, as a menu opened from the
    //  keyboard does.
    void  SetShowMnemonicCues (bool show)       { m_showCues = show; }

    //  The dimensions the menu lays out with, read from the Windows menu
    //  settings and refreshed on every DPI change. A caller that sets them
    //  explicitly, as a test does to keep its arithmetic off the host's
    //  display settings, pins them until `ClearMetrics`.
    void  SetMetrics   (const DxuiMenuMetrics & m)  { m_metrics = m; m_metricsPin = true; }
    void  ClearMetrics ()                           { m_metricsPin = false; RefreshMetrics(); }

    const DxuiMenuMetrics &  GetMetrics () const    { return m_metrics; }

    //  How long the pointer must rest on a submenu row before its child
    //  opens. Seeded from the system's menu show delay. Zero opens on
    //  contact, which is what this widget used to do.
    void  SetSubmenuDelayMs (int ms)            { m_submenuDelayMs = ms; }
    int   GetSubmenuDelayMs () const            { return m_submenuDelayMs; }

    //  Skip the open animation for the NEXT show. The menu bar sets this
    //  while walking from one title to the next: the animation marks entering
    //  menu mode, and replaying it per title would put a stutter on a sweep
    //  along the bar.
    void  SetRevealSuppressed (bool on)         { m_revealSuppressed = on; }

    //  True while a submenu is waiting out its delay. The pointer is not
    //  moving while it waits, so the host's idle loop has nothing to wake it
    //  and must keep ticking on its own until this goes false.
    bool  WantsTick () const;

    //  The clock the reopen guard reads. Defaults to the tick count; a test
    //  installs its own so the guard window can be crossed without waiting.
    void  SetClock    (ClockFn fn)              { m_clock = std::move (fn); }

    //  Whether a hosted popup takes mouse capture. A menu bar turns this off
    //  so the strip still sees the pointer and can swap titles on hover.
    void  SetGrabsCapture (bool grabs)          { m_grabsCapture = grabs; }

    //  The narrowest the next show may be, in pixels, so a menu hung from a
    //  wide control -- an address bar's history -- can match its width. Zero
    //  lets the content decide, and a shared menu should be given it again
    //  before each show, or one caller's width carries into the next.
    void  SetMinWidthPx (int px)                { m_minWidthPx = (px > 0) ? px : 0; }

    //  The tallest the menu may be, in pixels. A menu taller than this shows
    //  as many whole rows as fit and scrolls: by the wheel, or by moving the
    //  highlight past an edge. Unset, a hosted menu takes the room between
    //  what opened it and the bottom of its monitor, so a long list hangs
    //  under its anchor rather than flipping above it. A test pins it, since
    //  it has no monitor.
    void  SetMaxHeightPx (int px)               { m_maxHeightPx = (px > 0) ? px : 0; }

    int   GetScrollRow   () const               { return m_scrollRow; }
    bool  IsScrollable   () const               { return m_viewportPx > 0; }
    void  ScrollByRows   (int rows);

    void              SetPopupHost   (DxuiHwndSource * host) { m_popupHost = host; }
    DxuiHwndSource *  GetPopupHost   () const { return m_popupHost;   }
    DxuiPopupHost  *  GetActivePopup () const { return m_activePopup; }

    bool                                    IsVisible    () const { return m_visible; }
    bool                                    HasOpenChild () const;
    const DxuiPopupMenu                   * GetChild     () const { return m_child.get(); }
    int                                     GetHighlight () const { return m_hover; }
    const std::vector<Item>               & GetItems     () const { return m_legacyItems; }
    const std::vector<DxuiPopupMenuItem>  & GetRows      () const { return m_rows; }
    const RECT                            & GetRect      () const { return m_boundsDip; }

    void  ShowUnder      (const RECT                     & anchor,
                          std::vector<DxuiPopupMenuItem>   items,
                          IDxuiTextRenderer              & text,
                          const RECT                     & hostClient);
    void  ShowAt         (int                              x,
                          int                              y,
                          std::vector<DxuiPopupMenuItem>   items,
                          IDxuiTextRenderer              & text,
                          const RECT                     & hostClient);
    void  Show           (int                              anchorX,
                          int                              anchorY,
                          std::vector<Item>                items,
                          IDxuiTextRenderer              & text,
                          const RECT                     & hostClient);
    void  Hide           ();

    bool  HitTest        (int x, int y) const;

    //  For an owner that keeps its own gesture rules, as the menu bar does:
    //  which row is under a point, moving the highlight, and picking a row
    //  outright. The row index is the item index, separators included.
    int   HitTestRow     (int x, int y) const   { return HitTestIndex (x, y); }

    //  Which of an icon row's buttons is under a point, or -1.
    int  HitTestIconButton (int x, int y) const
    {
        int  row = HitTestIndex (x, y);

        return (row >= 0) ? GetIconButtonAt (row, x - m_boundsDip.left) : -1;
    }

    int  GetIconHighlight () const              { return m_iconHover; }
    void  SetHighlight   (int index)            { SetHover (index); }
    void  HighlightFirst ()                     { SetHover (FindFirstSelectable()); }
    void  ActivateRow    (int index)            { Commit (index); }

    //  The reopen guard exists for an opener whose hosted popup dismisses
    //  itself on the click BEFORE the opener sees that click. An opener that
    //  sees the click first, as the menu bar does because its popup takes no
    //  capture, toggles correctly on its own and turns the guard off, since
    //  it would otherwise refuse a legitimate reopen right after a pick.
    void  SetReopenGuard (bool enabled)         { m_reopenGuard = enabled; }

    void  Tick           (int64_t nowMs) override;
    void  OnMouseMove    (int x, int y);
    bool  OnLButtonDown  (int x, int y);
    bool  OnLButtonUp    (int x, int y);
    bool  OnKey          (WPARAM vk);
    void  Paint          (IDxuiPainter & painter, IDxuiTextRenderer & text) const;

    void                Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;
    bool                OnKey             (const DxuiKeyEvent   & ev) override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Dropdown; }

    //  Public so the menu bar, which still strips mnemonics for its titles,
    //  and this widget, which strips them for its rows, share one parser.
    //  "&&" collapses to a literal "&" and never marks a mnemonic.
    static void  ParseMnemonic (const std::wstring & label,
                                std::wstring       & outStripped,
                                int                & outIndex,
                                wchar_t            & outLower);

private:
    static constexpr int       kBorderDip              = 1;
    static constexpr int       kFallbackGlyphWidthDip  = 8;
    static constexpr float     kUnderlineThicknessDip  = 1.0f;
    static constexpr uint64_t  kReopenGuardMs          = 250;
    static constexpr int       kRevealMs               = 150;

    //  Fewer rows than this fit below the anchor, and a hosted menu flips
    //  above it rather than scroll in a sliver.
    static constexpr int       s_kMinRowsBelowAnchor   = 5;
    static constexpr int       s_kWheelRows            = 3;
    static constexpr float     s_kThumbWidthDip        = 3.0f;

    //  An icon row's buttons are square, a half again as tall as a command
    //  row, with their glyph at the toolbar's size.
    static constexpr int       s_kIconRowScalePct      = 150;
    static constexpr float     s_kIconGlyphDip         = 16.0f;
    static constexpr const wchar_t *  s_kIconFace      = L"Segoe Fluent Icons";

    //  The hover highlight is a rounded card inset from the menu's edges, not
    //  a full-bleed band: a square band running into the menu's own rounded
    //  corners reads as a stripe painted across the popup.
    static constexpr int       kHoverInsetXDip         = 4;
    static constexpr int       kHoverInsetYDip         = 2;
    static constexpr float     kHoverRadiusDip         = 4.0f;

    //  A submenu row's chevron, drawn as Explorer draws it: two thin strokes
    //  meeting at a point, in a box of fixed width at the row's right.
    static constexpr int       s_kSubmenuChevronBoxDip    = 12;
    static constexpr float     s_kSubmenuChevronHalfDip   = 4.0f;
    static constexpr float     s_kSubmenuChevronDepthDip  = 4.0f;
    static constexpr float     s_kSubmenuChevronStrokeDip = 1.0f;
    static constexpr float     s_kSubmenuChevronInsetDip  = 4.0f;

    struct Palette
    {
        uint32_t  bg       = 0;
        uint32_t  hover    = 0;
        uint32_t  text     = 0;
        uint32_t  accel    = 0;
        uint32_t  border   = 0;
        uint32_t  divider  = 0;
        uint32_t  disabled = 0;
    };

    //  Where a child hangs relative to its parent row, or where a root hangs
    //  relative to what opened it.
    enum class Anchoring
    {
        Below,
        AtPoint,
        Beside,
    };

    static bool  IsPointInRect (const RECT & rc, int x, int y);

    void  RefreshMetrics     ();

    bool  IsSelectable       (int index) const;
    int   FindNextSelectable (int from, int direction) const;
    int   FindFirstSelectable () const;
    bool  IsReopenSuppressed (const RECT & anchor) const;

    int   GetRowHeightPx     (int index) const;
    int   GetIconButtonPx    () const;
    int   GetIconButtonAt    (int index, int localX) const;
    void  CommitIcon         (int index, int button);
    void  PlaceIconRow       (bool atBottom);
    bool  OpensUpward        (int originX, int originY, int heightPx, Anchoring anchoring) const;
    int   GetRowTopPx        (int index) const;
    int   GetContentHeightPx () const;
    int   GetScrollPx        () const;
    int   GetMaxScrollRow    () const;
    int   GetHeightLimitPx   (const RECT & anchor, Anchoring anchoring) const;
    void  EnsureRowVisible   (int index);
    int   MeasureRunPx       (const std::wstring & run, float fontDip, IDxuiTextRenderer & text) const;
    int   MeasureWidthPx     (IDxuiTextRenderer & text);
    int   GetRowAtOffset     (int relY) const;
    int   HitTestIndex       (int x, int y) const;

    void  ShowCore           (int                              originX,
                              int                              originY,
                              const RECT                     & anchor,
                              Anchoring                        anchoring,
                              std::vector<DxuiPopupMenuItem>   items,
                              IDxuiTextRenderer              & text,
                              const RECT                     & hostClient);
    void  AcquirePopup       (const RECT & anchor, Anchoring anchoring);
    void  SetHover           (int index);
    void  ArmChild           (int index);
    void  DisarmChild        ();
    void  OpenChild          (int index, bool highlightFirst);
    void  CloseChild         ();
    void  Commit             (int index);
    DxuiPopupMenu *  GetRoot ();

    Palette  ResolvePalette  () const;
    void     PaintBody       (IDxuiPainter & painter, IDxuiTextRenderer & text, int originLeft, int originTop) const;
    void     PaintRow        (IDxuiPainter & painter, IDxuiTextRenderer & text, const Palette & pal,
                              int index, float left, float top, float width, float fontDip) const;
    void     PaintIconRow    (IDxuiPainter & painter, IDxuiTextRenderer & text, const Palette & pal,
                              int index, float left, float top) const;
    void     PaintUnderline  (IDxuiPainter & painter, IDxuiTextRenderer & text, const std::wstring & stripped,
                              int mnIdx, float labelX, float labelY, float fontDip, uint32_t ink) const;
    void     RenderPopupMenu (IDxuiPainter & painter, IDxuiTextRenderer & text) const;
    void     OnPopupMove     (POINT localPx);
    void     OnPopupClick    (POINT localPx);


    std::vector<DxuiPopupMenuItem>              m_rows;
    std::vector<Item>                           m_legacyItems;
    std::vector<std::shared_ptr<DxuiCommand>>   m_ownedCommands;
    std::unique_ptr<DxuiPopupMenu>              m_child;
    DxuiPopupMenu                             * m_parent      = nullptr;
    int                                         m_childRow    = -1;

    SelectFn             m_onSelect;
    SelectFn             m_onHighlight;
    ClosedFn             m_onClosed;
    std::function<void (POINT)>  m_onClickOutside;
    ClockFn              m_clock;
    bool                 m_committing       = false;
    const IDxuiTheme   * m_theme            = nullptr;
    IDxuiTextRenderer  * m_text             = nullptr;
    int                  m_hover            = -1;
    int                  m_iconHover        = -1;
    int                  m_pressed          = -1;
    bool                 m_visible          = false;
    bool                 m_revealSuppressed = false;
    bool                 m_hasGutter        = false;
    int                  m_labelLeftPx      = 0;
    int                  m_accelLeftPx      = 0;
    int                  m_accelWidthPx     = 0;
    bool                 m_showCues         = false;
    RECT                 m_hostClient       = {};
    RECT                 m_anchor           = {};
    RECT                 m_lastAnchor       = {};
    int                  m_submenuDelayMs   = 0;
    int                  m_pendingChild     = -1;
    uint64_t             m_pendingAtMs      = 0;
    uint64_t             m_closedAtMs       = 0;
    bool                 m_hasClosed        = false;
    DxuiDpiScaler        m_scaler;
    DxuiMenuMetrics      m_metrics;
    bool                 m_metricsPin       = false;
    DxuiHwndSource     * m_popupHost        = nullptr;
    DxuiPopupHost      * m_activePopup      = nullptr;
    bool                 m_grabsCapture     = true;
    int                  m_minWidthPx       = 0;
    int                  m_maxHeightPx      = 0;
    int                  m_viewportPx       = 0;
    int                  m_scrollRow        = 0;
    bool                 m_reopenGuard      = true;

    bool                 m_colorsSet   = false;
    Palette              m_colors;
};
