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
//  Every entry holds a command by pointer and reads its label, glyph, tip,
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
        const DxuiCommand        * command    = nullptr;
        Kind                       kind       = Kind::Command;
        int                        group      = 0;
        DecorationFn               decoration;
        IDxuiToolbarCustomEntry  * custom     = nullptr;
    };

    DxuiToolbar  ();
    ~DxuiToolbar () override;

    void  SetEntries       (std::vector<Entry> entries);
    void  SetIconFace      (const wchar_t * face)        { m_iconFace = face; }
    void  SetTextRenderer  (IDxuiTextRenderer * text)    { m_textRenderer = text; }
    void  SetStripColors   (uint32_t stripArgb, uint32_t textArgb);
    void  ClearStripColors ()                            { m_stripColorsSet = false; }

    //  Decides how many entries can still afford their label at this width
    //  and returns the band thickness (dp) the strip needs. Call BEFORE
    //  docking the chrome bands.
    int   PlanForWidth     (int clientWidthPx, const DxuiDpiScaler & scaler);
    int   GetBandDp        () const;
    bool  IsLabeled        (int commandId) const;

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
    static constexpr float     kFontDip          = 13.0f;
    static constexpr float     kFallbackCharPx   = 7.5f;
    static constexpr uint32_t  kDisabledInkAlpha = 0x60000000u;

    //  Runtime state the strip keeps per entry.
    struct Slot
    {
        Entry  entry;
        RECT   rc      = {};
        bool   hovered = false;
        bool   pressed = false;
        bool   labeled = true;
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

    const Slot *  FindSlot             (int commandId) const;
    Slot       *  FindSlot             (int commandId);
    int           MeasureLabelPx       (const wchar_t * text, float fontPx) const;
    int           GetEntryWidthPx      (const Slot & slot, bool labeled) const;
    int           GetTotalWidthPx      (int labeledCount) const;
    RECT          GetFlyoutKeepAliveRc () const;
    void          LayoutFlyout         ();
    void          OpenFlyout           (bool byKeyboard);
    void          CloseFlyout          ();
    void          OpenDropDown         (int commandId);

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

    IDxuiTextRenderer      * m_textRenderer   = nullptr;
    const wchar_t          * m_iconFace       = L"Segoe MDL2 Assets";
    RECT                     m_barRect        = {};
    RECT                     m_hostClient     = {};
    DxuiDpiScaler            m_scaler;
    int                      m_labeledCount   = 0;

    bool                     m_stripColorsSet = false;
    uint32_t                 m_stripOverride  = 0;
    uint32_t                 m_textOverride   = 0;
};
