#include "Pch.h"

#include "MainMenu.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MainMenu::MainMenu
//
//  The item lists hold the table's commands by pointer; the table is a
//  member, so they live exactly as long as the bar does.
//
////////////////////////////////////////////////////////////////////////////////

MainMenu::MainMenu()
{
    SetItems (m_commands.BuildMenuItems());
}





////////////////////////////////////////////////////////////////////////////////
//
//  MainMenu::~MainMenu
//
////////////////////////////////////////////////////////////////////////////////

MainMenu::~MainMenu()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  MainMenu::Show
//
////////////////////////////////////////////////////////////////////////////////

void MainMenu::Show()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  MainMenu::Open
//
////////////////////////////////////////////////////////////////////////////////

void MainMenu::Open (MainMenuId menu, bool openedByKeyboard)
{
    DxuiMenuBar::Open ((int) menu, openedByKeyboard);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MainMenu::PaintStrip
//
////////////////////////////////////////////////////////////////////////////////

void MainMenu::PaintStrip (
    DxuiPainter             & painter,
    DxuiTextRenderer        & text,
    const ChromeVisualState & visual,
    const CassoTheme       & theme)
{
    ApplyChromeColors (theme);
    DxuiMenuBar::PaintStrip (painter, text, theme, visual.dpi);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MainMenu::PaintDropdown
//
////////////////////////////////////////////////////////////////////////////////

void MainMenu::PaintDropdown (
    DxuiPainter             & painter,
    DxuiTextRenderer        & text,
    const ChromeVisualState & visual,
    const CassoTheme       & theme)
{
    ApplyChromeColors (theme);
    DxuiMenuBar::PaintDropdown (painter, text, theme, visual.dpi);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MainMenu::ApplyChromeColors
//
//  Pushes the Casso-specific nav / dropdown palette onto the menu bar
//  so visual parity with the legacy chrome is preserved across paint.
//  The generic `IDxuiTheme` mapping is close but not identical, so we
//  drive the overrides every frame; cheap and tolerant of theme swaps.
//
////////////////////////////////////////////////////////////////////////////////

void MainMenu::ApplyChromeColors (const CassoTheme & theme)
{
    SetStripColors    (theme.navStrip,
                       theme.navHover,
                       theme.navItemText);
    SetDropdownColors (theme.dropdownBg,
                       theme.dropdownHover,
                       theme.dropdownItemText,
                       theme.dropdownAccel,
                       theme.navHover,
                       theme.navHover);
}
