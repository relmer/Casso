#pragma once

#include "Pch.h"
#include "Core/DxuiCommand.h"
#include "Core/IDxuiControl.h"
#include "Widgets/DxuiPopupMenu.h"



class DxuiHwndSource;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarIconBox
//
//  Where an entry's icon sits, in the fractional pixels the glyph is drawn
//  at: a collapsed entry centers its icon in an integer rect, so the box's
//  left edge is a half pixel as often as not, and anything drawn against it
//  has to use the same edge or land a half pixel off.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiToolbarIconBox
{
    float  x    = 0.0f;   // icon left edge
    float  top  = 0.0f;   // entry top; the icon is centered in rowH
    float  size = 0.0f;   // icon em, px
    float  rowH = 0.0f;   // entry height, px
};





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiToolbarCustomEntry
//
//  An entry that draws itself. The toolbar still owns its place on the
//  strip, its collapse schedule and its hover and press state; everything
//  inside its rect is the entry's. Width, layout and paint are asked for in
//  both forms, since a custom entry collapses to an icon like any other.
//
//  OnClick reports whether the click was consumed. When it returns false the
//  toolbar acts on the entry's kind, so a collapsed entry can offer a
//  drop-down of the choices its expanded form shows as segments.
//
//  The pointer hooks are optional. An entry with sub-parts that light on
//  hover overrides OnMouseMove and reports whether the pointer is over one;
//  an entry whose sub-parts take a press overrides OnLButtonDown and reports
//  that it did, which arms the entry for the OnClick that follows.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiToolbarCustomEntry
{
public:
    virtual ~IDxuiToolbarCustomEntry() = default;

    virtual int              GetWidthPx    (bool labeled, const DxuiDpiScaler & scaler, IDxuiTextRenderer * text) const = 0;
    virtual void             Layout        (const RECT & rc, bool labeled, const DxuiDpiScaler & scaler)                = 0;
    virtual void             Paint         (IDxuiPainter      & painter,
                                            IDxuiTextRenderer & text,
                                            const IDxuiTheme  & theme,
                                            bool                hovered,
                                            bool                pressed,
                                            bool                labeled)                                               = 0;
    virtual const wchar_t *  GetTooltipAt  (int x, int y, RECT & anchor) const                                          = 0;
    virtual bool             OnClick       (int x, int y)                                                               = 0;

    virtual bool             OnMouseMove   (int x, int y)                                                               { (void) x; (void) y; return false; }
    virtual void             OnMouseLeave  ()                                                                           {}
    virtual bool             OnLButtonDown (int x, int y)                                                               { (void) x; (void) y; return false; }
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbar
//
//  A strip of commands: icon-plus-label buttons that are frameless until
//  hovered or pressed, collapsing their labels ONE AT A TIME FROM THE RIGHT
//  when the strip runs out of room, so the leftmost keep their names longest
//  and no entry ever falls off the end.
//
//  Every entry shares ownership of its command and reads its label, glyph, tip,
//  checked and enabled state from it AT PAINT AND CLICK TIME. What a click
//  does is the entry's kind: a Command dispatches, a Toggle dispatches and
//  draws pressed while checked, a DropDown opens a menu of commands with
//  preview and commit sinks, a Flyout dispatches and opens a panel hosting a
//  control the application owns on dwell. A decoration paints over an
//  entry's icon without knowing what it draws; a custom entry owns
//  everything inside its rect.
//
//  Input is hand-routed by the host: it forwards mouse events to the
//  OnToolbar* handlers with client coordinates, and every keydown while a
//  menu is open, so arrowing through rows previews instead of typing into
//  whatever is behind the strip.
//
//  Colors come from IDxuiTheme. The strip fill and label ink can be
//  overridden for a host whose chrome palette the generic mapping does not
//  carry, as the menu bar's can.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiToolbar : public IDxuiControl
{
public:
    enum class Kind
    {
        Command,
        Toggle,
        DropDown,
        Flyout,
    };

    using ChoiceFn     = std::function<void (int index)>;
    using DecorationFn = std::function<void (IDxuiPainter              & painter,
                                             const IDxuiTheme          & theme,
                                             const DxuiToolbarIconBox  & icon,
                                             bool                        collapsed)>;

    struct Entry
    {
        std::shared_ptr<const DxuiCommand>   command;
        Kind                                 kind       = Kind::Command;
        int                                  group      = 0;
        DecorationFn                         decoration;
        IDxuiToolbarCustomEntry            * custom     = nullptr;

        //  Never labeled, regardless of available width, like Explorer's Back,
        //  Forward, Up and Refresh buttons. The tooltip text is unchanged.
        bool  iconOnly = false;

        //  Sits at the strip's trailing end rather than after the entry
        //  before it, like Explorer's Details button. Trailing entries come
        //  last in the list; with no room to spare they close up normally.
        bool  trailing = false;

        //  Lives in the See more menu whatever the room, never on the strip,
        //  as Explorer keeps its rarer commands there.
        bool  seeMoreOnly = false;
    };

    DxuiToolbar  ();
    ~DxuiToolbar () override;

    void  SetEntries       (std::vector<Entry> entries);

    //  Ends the leading entries in a See more button, as Explorer's command
    //  bar ends: entries that no longer fit even as icons move into its menu
    //  from the right as the strip narrows and come back as it widens, and
    //  entries marked seeMoreOnly are always there. The button shows only
    //  when its menu has something in it. Call before SetEntries.
    void  EnableSeeMore    (const wchar_t * glyph, const wchar_t * tip);
    bool  IsInSeeMore      (int commandId) const;

    //  The id on the See more button's command.
    static constexpr int  kSeeMoreId = -2;
    void  SetIconFace      (const wchar_t * face)        { m_iconFace = face; }
    void  SetIconDip       (float dip)                   { m_iconDip = dip; }

    //  Whether a drop-down that shows an icon also shows the chevron that
    //  says it opens a menu. One without an icon always shows it. File
    //  Explorer draws it on both, so a toolbar following Explorer sets this;
    //  the emulator's own chrome does not.
    void  SetChevronOnIcons (bool on)                    { m_chevronOnIcons = on; }

    //  The two icon fonts for the glyphs in UnicodeSymbols.h. They use the same
    //  code points; Windows 11 uses Fluent for its own chrome, which draws some
    //  glyphs differently (Refresh most visibly) and is not in Windows 10.
    static constexpr const wchar_t *  kMdl2IconFace   = L"Segoe MDL2 Assets";
    static constexpr const wchar_t *  kFluentIconFace = L"Segoe Fluent Icons";
    void  SetTextRenderer  (IDxuiTextRenderer * text)    { m_textRenderer = text; }
    void  SetStripColors   (uint32_t stripArgb, uint32_t textArgb);
    void  ClearStripColors ()                            { m_stripColorsSet = false; }

    //  Decides how many entries can still afford their label at this width
    //  and returns the band thickness (dp) the strip needs. Call BEFORE
    //  docking the chrome bands.
    int   PlanForWidth     (int clientWidthPx, const DxuiDpiScaler & scaler);
    int   GetBandDp        () const;
    bool  IsLabeled        (int commandId) const;
    bool  TryGetEntryRect  (int commandId, RECT & outRect) const;

    //  The span between the last leading entry and the first trailing one, a
    //  group gap from each, for a host control such as an address bar. Empty
    //  when there is no room.
    RECT  GetFreeRect      () const                      { return m_freeRect; }

    //  As the menu bar's: the open picker's submenu delay needs a heartbeat.
    bool  WantsTick () const { return m_dropdown.WantsTick(); }
    void  TickMenus (int64_t nowMs) { m_dropdown.Tick (nowMs); }

    void  Layout           (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint            (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

    //  Host-forwarded pointer input, client coordinates. True when consumed.
    bool             OnToolbarMouseMove   (int x, int y);
    void             OnToolbarMouseLeave  ();
    bool             OnToolbarLButtonDown (int x, int y);
    bool             OnToolbarLButtonUp   (int x, int y);
    const wchar_t *  GetTooltipAt         (int x, int y, RECT & anchor) const;
    bool             HitTest              (int x, int y) const;

    //  Keyboard access. The host's focus ring hands the strip a focused entry
    //  index (-1 for none) and every keydown while the strip owns the
    //  keyboard, which is while a menu is open or a flyout was opened by
    //  keyboard. ActivateFocused is Enter on the focused entry: a Command
    //  dispatches, a DropDown opens its list, a Flyout opens its panel and
    //  gives the hosted control focus, so arrows reach it; Escape closes the
    //  panel and leaves the entry focused.
    int   GetEntryCount    () const                      { return (int) m_slots.size(); }

    //  The command at a strip position, See more's included, and whether the
    //  entry there is on the strip rather than in See more's menu.
    int   GetEntryCommandId (int index) const            { return (index >= 0 && index < (int) m_slots.size() && m_slots[(size_t) index].entry.command != nullptr) ? m_slots[(size_t) index].entry.command->id : 0; }
    bool  IsEntryShown      (int index) const            { return index >= 0 && index < (int) m_slots.size() && !m_slots[(size_t) index].hidden; }
    void  SetFocusIndex    (int index);
    int   GetFocusIndex    () const                      { return m_focusIndex; }
    void  ActivateFocused  ();
    bool  IsMenuOpen       () const;
    bool  OwnsKeyboard     () const;
    bool  HandleKey        (WPARAM vk);

    //  A drop-down's rows are commands too, typically with `isChecked`
    //  reading the application's current choice. Preview fires as the
    //  highlight moves and persists nothing; commit fires once on selection;
    //  a dismissal replays preview with the row the menu opened on, which is
    //  the snap-back.
    void  SetDropDownItems (int commandId, std::vector<DxuiPopupMenuItem> items);

    // Where a click that dismissed a drop-down landed, in screen pixels, so
    // the owner can act on it. See DxuiPopupHost::Params::onClickOutside.
    void  SetDropDownClickOutsideFn (std::function<void (POINT screenPx)> fn)
              { m_onDropDownClickOutside = std::move (fn); }
    void  SetDropDownSinks (int commandId, ChoiceFn preview, ChoiceFn commit);

    //  A flyout opens on dwell over its entry and closes when the pointer
    //  leaves the union of entry and panel, unless a press on the hosted
    //  control is in progress. The control is laid out inside the panel's
    //  padding and painted after everything on the strip.
    void  SetFlyoutControl (int commandId, IDxuiControl * control, SIZE panelDp);
    bool  IsFlyoutOpen     (int commandId) const;

    void  SetPopupHost      (DxuiHwndSource * host);
    void  SetHostClientRect (const RECT & clientRect)    { m_hostClient = clientRect; }

    //  The drop-down's reopen guard runs on this clock; a test injects one
    //  so the close window is crossed by arithmetic rather than by sleeping.
    void  SetClock          (DxuiPopupMenu::ClockFn fn)  { m_dropdown.SetClock (std::move (fn)); }

private:
    static constexpr int       kBarPadXDp        = 10;   // strip left/right padding
    static constexpr int       kBtnPadXDp        = 10;   // inside a button, around content
    static constexpr int       kBtnMarginYDp     = 5;    // button top/bottom inset in the strip
    static constexpr int       kBtnGapDp         = 4;    // between buttons in a group
    static constexpr int       kGroupGapDp       = 18;   // between button groups
    static constexpr int       kIconGapDp        = 7;    // icon-to-label gap
    static constexpr int       kBandDp           = 42;   // strip thickness
    static constexpr int       kFlyoutPadDp      = 8;
    static constexpr int       kFlyoutDropDp     = 2;    // gap under the bar
    static constexpr float     kIconDip          = 15.0f;
    static constexpr float     kFallbackFontDip  = 13.0f;  // size the char estimate was taken at
    static constexpr float     kFallbackCharPx   = 7.5f;
    static constexpr uint32_t  kDisabledInkAlpha = 0x60000000u;

    //  The chrome font: one size for the strip's labels, the menu bar's
    //  titles and every dropdown, read from the Windows menu settings. A
    //  toolbar label in a font its OWN picker did not use is the mismatch
    //  this avoids -- the pickers are popup menus and paint in that font.
    float  GetChromeFontPx () const { return m_metrics.fontPx; }
    void   RefreshMetrics  ();

    //  Runtime state the strip keeps per entry.
    struct Slot
    {
        Entry  entry;
        RECT   rc      = {};
        bool   hovered = false;
        bool   pressed = false;
        bool   labeled = true;
        bool   hidden  = false;   // in the See more menu rather than on the strip
    };

    struct Picker
    {
        std::vector<DxuiPopupMenuItem>  items;
        ChoiceFn                        preview;
        ChoiceFn                        commit;
        int                             openedOn  = -1;
        bool                            previewed = false;
    };

    static bool  IsPointInRect (const RECT & rc, int x, int y);

    //  An entry without a glyph is its label alone: it spends no room on an
    //  icon, never collapses to one, and as a drop-down shows a chevron.
    static bool  HasGlyph (const Slot & slot) { return slot.entry.command != nullptr && slot.entry.command->glyph != nullptr && slot.entry.command->glyph[0] != 0; }

    static constexpr int  kChevronDp = 8;

    const Slot *  FindSlot             (int commandId) const;
    Slot       *  FindSlot             (int commandId);
    int           MeasureLabelPx       (const wchar_t * text, float fontPx) const;
    int           GetEntryWidthPx      (const Slot & slot, bool labeled) const;
    int           GetTotalWidthPx      (int labeledCount) const;
    RECT          GetFlyoutKeepAliveRc () const;
    void          LayoutFlyout         ();
    void          PlaceTrailingEntries (int rightPx);
    void          OpenFlyout           (bool byKeyboard);
    void          CloseFlyout          ();
    void          OpenDropDown         (int commandId);
    void          OpenSeeMore          ();
    void          PlanSeeMore          (int clientWidthPx);

    std::function<void (POINT)>  m_onDropDownClickOutside;
    void          WireDropDown         ();
    void          ForwardToFlyout      (DxuiMouseEventKind kind, DxuiMouseButton button, int x, int y, bool & handled);

    void  PaintSlot      (Slot & slot, IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);
    void  PaintEntryIcon (const Slot & slot, IDxuiTextRenderer & text, const DxuiToolbarIconBox & icon, uint32_t ink);
    void  PaintFlyout    (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);


    std::vector<Slot>        m_slots;
    std::map<int, Picker>    m_pickers;
    DxuiPopupMenu            m_dropdown;
    int                      m_openPicker     = -1;

    IDxuiControl           * m_flyoutControl  = nullptr;
    SIZE                     m_flyoutPanelDp  = {};
    int                      m_flyoutId       = -1;
    bool                     m_flyoutOpen     = false;
    bool                     m_flyoutKeyboard = false;   // opened by Enter: the pointer cannot close it
    bool                     m_flyoutPressed  = false;
    RECT                     m_flyoutRc       = {};
    int                      m_focusIndex     = -1;

    IDxuiTextRenderer             * m_textRenderer   = nullptr;
    const wchar_t                 * m_iconFace       = kMdl2IconFace;
    float                           m_iconDip        = kIconDip;
    bool                            m_chevronOnIcons = false;
    RECT                            m_barRect        = {};
    RECT                            m_freeRect       = {};
    RECT                            m_hostClient     = {};
    DxuiDpiScaler                   m_scaler;
    DxuiMenuMetrics                 m_metrics;
    int                             m_labeledCount   = 0;
    std::shared_ptr<DxuiCommand>    m_seeMore;

    bool                     m_stripColorsSet = false;
    uint32_t                 m_stripOverride  = 0;
    uint32_t                 m_textOverride   = 0;
};
