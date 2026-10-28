#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerCommands.h"

struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerToolbars
//
//  The window's bars: the navigation toolbar, with the address and search
//  boxes in its free span; Explorer's command bar, with buttons for the
//  location and the Sort, Group by and View drop-downs; and the hidden menu
//  bar. The window routes pointer events over the bars here, with their
//  tooltip, which is muted after a click.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerToolbars
{
public:
    explicit ExplorerToolbars (ExplorerContext & context);

    //  Explorer's navigation glyphs are smaller than Casso's toolbar icons:
    //  15 pixels of ink at 120 DPI.
    static constexpr float  kNavIconDip              = 12.0f;

    //  Its back, forward, up and refresh buttons are 48 dip apart, center to
    //  center, the first 34.67 dip from the window's left edge: measured at
    //  150%. A 12 dip glyph, 16 dip either side and the toolbar's 4 dip gap.
    static constexpr float  kNavButtonPadDip         = 16.0f;
    static constexpr int    kNavBarPadDp             = 5;
    static constexpr int    kNavAddressGapDp         = 8;      // from the refresh button to the address box

    //  File Explorer's command bar, measured at 125%: buttons 48 dip apart
    //  center to center, label cap height 11 px against the chrome font's 13,
    //  and its 20-unit Fluent icons drawn one unit to the dip -- the scissors
    //  are 20 pixels tall. A 20 dip square, 12 dip either side and the
    //  toolbar's 4 dip gap keep the 48.
    static constexpr float  kCommandBarIconDip       = 20.0f;
    static constexpr float  kCommandBarPadDip        = 12.0f;
    static constexpr float  kCommandBarLabeledDip    = 10.0f;   // a labeled button's pad before its icon, measured at 150%
    static constexpr float  kCommandBarDropDip       = 0.67f;   // Explorer's buttons sit a pixel below the bar's middle at 150%
    static constexpr float  kCommandBarLabelDip      = 12.0f;
    static constexpr float  kCommandBarChevronGapDip = 6.0f;    // a drop-down's label to its chevron, measured at 150%
    static constexpr float  kCommandBarIconGapDip    = 6.33f;   // a button's icon to its label, measured at 150%

    static constexpr int    kCommandBarGroupGapDp    = 9;    // 14 px at 150%, with the separator centered in it
    static constexpr int    kCommandBarPadXDp        = 6;

    //  The bars, as children of the window, each created where it falls in the
    //  window's paint order; their setup; and their settings once the window
    //  has its HWND.
    void  CreateNavBar     ();
    void  CreateMenuBar    ();
    void  CreateCommandBar ();
    void  Create     ();
    void  Configure  ();
    bool  IsCreated  () const { return m_menuBar != nullptr; }
    void  ApplyTheme (const DxuiTheme & theme);

    void  PlanForWidth (int widthPx);
    void  Layout       (const RECT & commandBand, const RECT & navBand);
    RECT  GetFreeRect  () const { return m_toolbar->GetFreeRect(); }

    //  The location's buttons, the drop-downs and the View button's icon.
    void  UpdateCommandBar       ();
    void  SetCommandBarDropDowns ();
    void  ShowViewOnButton       ();

    //  Pointer events. A press outside a bar with a menu open closes it, ahead
    //  of the window's other routing; the menu bar is ahead of the address
    //  bar, and the two bars after it.
    void  CloseMenusOutside (const DxuiMouseEvent & ev);
    void  CloseMenus        ();
    bool  HandleMenuBarMouse    (const DxuiMouseEvent & ev) { return m_menuBar->OnMouse (ev); }
    bool  HandleMouse           (const DxuiMouseEvent & ev);
    bool  RouteToolbarMouse (DxuiToolbar & toolbar, const DxuiMouseEvent & ev);
    bool  TryMenuBarKey     (const DxuiKeyEvent & ev, bool & outHandled);

    //  The strip a pointer event goes to: the command bar over its band, the
    //  navigation toolbar anywhere else.
    static DxuiToolbar &  GetToolbarUnder (const RECT & commandBarBand, POINT point, DxuiToolbar & navToolbar, DxuiToolbar & commandBar);

    bool  WantsTick  () const;
    bool  IsMenuOpen () const;
    void  TickMenus  (int64_t nowMs);

    //  The bars, for keyboard focus on their buttons.
    DxuiToolbar &  GetNavToolbar () const { return *m_toolbar; }
    DxuiToolbar &  GetCommandBar () const { return *m_commandBar; }

private:
    ExplorerContext  & m_context;

    DxuiToolbar          * m_toolbar    = nullptr;
    DxuiToolbar          * m_commandBar = nullptr;
    DxuiMenuBar          * m_menuBar    = nullptr;

    //  A button clicked has no tip until the pointer moves off it, so no tip
    //  is drawn over the menu it opened, as in Explorer's command bar.
    bool                   m_tipMuted    = false;
    RECT                   m_tipMuteRect = {};

    //  The location's buttons on the command bar, if any.
    CassoExplorerCommands::CommandBarExtra  m_commandBarExtra = CassoExplorerCommands::CommandBarExtra::None;
};