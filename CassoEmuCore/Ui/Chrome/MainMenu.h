#pragma once

#include "Pch.h"

#include "CassoTheme.h"
#include "EmulatorCommands.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MainMenu
//
//  Casso's application menu bar: a DxuiMenuBar whose titles and rows come
//  from the emulator's command table. The shell's dispatch, check, enable
//  and label queries are set on the table through this class, which is
//  where the shell already sets them; the toolbar reads the same table
//  through GetCommands.
//
//  Visual parity with the legacy chrome is preserved by mirroring the
//  Casso-specific palette (`CassoTheme::nav*Argb` etc.) onto the
//  menu bar via `DxuiMenuBar::SetStripColors` / `SetDropdownColors`
//  before each strip / dropdown paint.
//
////////////////////////////////////////////////////////////////////////////////

class MainMenu : public DxuiMenuBar
{
public:
    using DispatchFn = EmulatorCommands::DispatchFn;
    using CheckFn    = EmulatorCommands::CheckFn;
    using LabelFn    = EmulatorCommands::LabelFn;

    MainMenu  ();
    ~MainMenu () override;

    void  SetDispatch     (DispatchFn dispatch)   { m_commands.SetDispatch    (std::move (dispatch)); }
    void  SetCheckQuery   (CheckFn query)         { m_commands.SetCheckQuery  (std::move (query)); }
    void  SetEnableQuery  (CheckFn query)         { m_commands.SetEnableQuery (std::move (query)); }
    void  SetLabelQuery   (LabelFn query)         { m_commands.SetLabelQuery  (std::move (query)); }

    EmulatorCommands       &  GetCommands ()        { return m_commands; }
    const EmulatorCommands &  GetCommands () const  { return m_commands; }

    void  Show           ();
    void  Dispatch       (WORD commandId) const     { m_commands.Dispatch (commandId); }
    void  PaintStrip     (DxuiPainter             & painter,
                          DxuiTextRenderer        & text,
                          const ChromeVisualState & visual,
                          const CassoTheme       & theme);
    void  PaintDropdown  (DxuiPainter             & painter,
                          DxuiTextRenderer        & text,
                          const ChromeVisualState & visual,
                          const CassoTheme       & theme);
    void  Open           (MainMenuId menu, bool openedByKeyboard);
    using DxuiMenuBar::Open;     // expose int-indexed Open from base

    //
    //  Push the Casso nav/dropdown palette onto the menu bar. Must be
    //  driven on a live path (theme apply / switch) so both the in-window
    //  strip and the popup-backed dropdown use the chrome colors -- the
    //  generic IDxuiTheme mapping is close but not identical.
    //
    void  ApplyChromeColors (const CassoTheme & theme);

    MainMenuId  GetOpenMenu      () const { return (MainMenuId) OpenMenuIndex(); }
    MainMenuId  GetFocusedMenuId () const { return (MainMenuId) DxuiMenuBar::GetFocusedMenu(); }
    using DxuiMenuBar::GetFocusedMenu;

    void        SetFocusedMenu (MainMenuId menu) { DxuiMenuBar::SetFocusedMenu ((int) menu); }
    using DxuiMenuBar::SetFocusedMenu;

    static constexpr int  kMenuCount = EmulatorCommands::kMenuCount;

private:
    EmulatorCommands  m_commands;
};
