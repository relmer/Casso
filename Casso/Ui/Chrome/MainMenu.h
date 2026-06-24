#pragma once

#include "Pch.h"

#include "ChromeTheme.h"





enum class MainMenuId
{
    File    = 0,
    Edit    = 1,
    Machine = 2,
    Disk    = 3,
    View    = 4,
    Debug   = 5,
    Help    = 6,
};


struct MainMenuCommandEntry
{
    WORD            commandId;
    MainMenuId      menu;
    const wchar_t * label;
    const wchar_t * accelerator;
    bool            checkable = false;
};



////////////////////////////////////////////////////////////////////////////////
//
//  MainMenu
//
//  Casso's application menu bar. Configures a `DxuiMenuBar` with the
//  Casso command set and translates per-command dispatch / check-query
//  callbacks (WORD command id -> functor) into the generic per-subitem
//  callbacks the widget expects. Renamed when the legacy nav-strip menu
//  logic was promoted to the Dxui framework.
//
//  Visual parity with the legacy chrome is preserved by mirroring the
//  Casso-specific palette (`ChromeTheme::nav*Argb` etc.) onto the
//  menu bar via `DxuiMenuBar::SetStripColors` / `SetDropdownColors`
//  before each strip / dropdown paint.
//
////////////////////////////////////////////////////////////////////////////////

class MainMenu : public DxuiMenuBar
{
public:
    using DispatchFn = std::function<void (WORD commandId)>;
    using CheckFn    = std::function<bool (WORD commandId)>;

    MainMenu  ();
    ~MainMenu () override;

    void                                   SetDispatch        (DispatchFn dispatch);
    void                                   SetCheckQuery      (CheckFn query);

    static std::span<const MainMenuCommandEntry>  GetCommandEntries  ();
    static const wchar_t                       *  GetMenuName        (MainMenuId menu);
    static std::string                            EmitParityMarkdown ();
    static bool                                   IsSeparator        (const MainMenuCommandEntry & entry);

    void  Show           ();
    void  Dispatch       (WORD commandId) const;
    void  PaintStrip     (DxuiPainter             & painter,
                          DxuiTextRenderer        & text,
                          const ChromeVisualState & visual,
                          const ChromeTheme       & theme);
    void  PaintDropdown  (DxuiPainter             & painter,
                          DxuiTextRenderer        & text,
                          const ChromeVisualState & visual,
                          const ChromeTheme       & theme);
    void  Open           (MainMenuId menu, bool openedByKeyboard);
    using DxuiMenuBar::Open;     // expose int-indexed Open from base

    //
    //  Push the Casso nav/dropdown palette onto the menu bar. Must be
    //  driven on a live path (theme apply / switch) so both the in-window
    //  strip and the popup-backed dropdown use the chrome colours -- the
    //  generic IDxuiTheme mapping is close but not identical.
    //
    void  ApplyChromeColors (const ChromeTheme & theme);

    MainMenuId  OpenMenu       () const { return (MainMenuId) OpenMenuIndex(); }
    MainMenuId  FocusedMenuId  () const { return (MainMenuId) DxuiMenuBar::FocusedMenu(); }
    using DxuiMenuBar::FocusedMenu;

    void        SetFocusedMenu (MainMenuId menu) { DxuiMenuBar::SetFocusedMenu ((int) menu); }
    using DxuiMenuBar::SetFocusedMenu;

    static constexpr int  kMenuCount = 7;

private:
    void  Rebuild           ();


    DispatchFn  m_dispatch;
    CheckFn     m_isChecked;
};
