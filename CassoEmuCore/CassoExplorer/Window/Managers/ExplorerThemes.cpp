#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerThemes.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/CassoExplorerWindow.h"

//  The Theme menu's rows, in order, by the theme each one selects: Casso's
//  themes in Casso's order, then Windows' dark and light and Follow system.
static constexpr const char *  s_kpszThemeRows[] =
{
    CassoExplorerPrefs::kThemeSkeuomorphic,
    CassoExplorerPrefs::kThemeRetroTerminal,
    CassoExplorerPrefs::kThemeDarkModern,
    CassoExplorerPrefs::kThemeDark,
    CassoExplorerPrefs::kThemeLight,
    CassoExplorerPrefs::kThemeFollowSystem,
};





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerThemes::ExplorerThemes
//
////////////////////////////////////////////////////////////////////////////////

ExplorerThemes::ExplorerThemes (CassoExplorerWindow & window)
    : m_window (window)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerThemes::ChooseInitial
//
//  The theme from the preferences, before the window and its widgets exist.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerThemes::ChooseInitial()
{
    AdoptSystemColors();

    m_window.m_theme = &CassoExplorerShell::ChooseTheme (m_window.m_prefs.theme, DxuiWindowsThemeColors::Instance().IsDarkMode(), m_lightTheme, m_darkTheme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerThemes::HandleSystemThemeChanged
//
//  Applying a theme sends a theme change to the controls, the window
//  included, so a change that arrives while one is applied is ignored.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerThemes::HandleSystemThemeChanged()
{
    if (m_applying)
    {
        return;
    }

    m_applying = true;

    DxuiWindowsThemeColors::Instance().Refresh();
    Apply();
    m_window.Invalidate();

    m_applying = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerThemes::AdoptSystemColors
//
//  The Windows themes take the list surface and the accent from the system
//  each time a theme is chosen, which also picks up an accent changed while
//  the window was away.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerThemes::AdoptSystemColors()
{
    const DxuiWindowsThemeColors::SystemColors &  system = DxuiWindowsThemeColors::Instance().GetSystemColors();



    m_lightTheme.ApplySystemColors (system);
    m_darkTheme.ApplySystemColors  (system);

    //  The caption and the tab row are the window's Mica Alt, and the row under
    //  the tabs lightens it, as Explorer's do.
    m_lightTheme.titleBarTop    = 0x00000000u;
    m_darkTheme.titleBarTop     = 0x00000000u;
    m_lightTheme.titleBarBottom = 0x00000000u;
    m_darkTheme.titleBarBottom  = 0x00000000u;
    m_lightTheme.navStrip       = 0xB3FFFFFFu;
    m_darkTheme.navStrip        = 0x0EFFFFFFu;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerThemes::Apply
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerThemes::Apply()
{
    AdoptSystemColors();

    m_window.m_theme = &CassoExplorerShell::ChooseTheme (m_window.m_prefs.theme, DxuiWindowsThemeColors::Instance().IsDarkMode(), m_lightTheme, m_darkTheme);

    //  Casso's own themes lend their chrome colors; the skeuomorphic one has
    //  no scene to draw here, so it is colors only.
    if (IsCassoThemeName (m_window.m_prefs.theme))
    {
        m_cassoTheme = CassoTheme::MakeByName (m_window.m_prefs.theme);
        m_window.m_theme      = &m_cassoTheme;
    }

    //  The menus' icons come in a set per theme.
    m_window.m_menus.ApplyMenuSvgs();

    m_window.SetTheme (m_window.m_theme);

    m_window.m_toolbars.ApplyTheme (*m_window.m_theme);

    if (m_window.m_toolbars.IsCreated())
    {
        m_window.m_tips.ApplyTheme (*m_window.m_theme);
        m_window.m_addressBar.ApplyTheme (m_window.m_theme);
    }

    m_window.m_tabManager.ApplyTheme (*m_window.m_theme);

    if (m_window.GetHwnd() != nullptr)
    {
        uint32_t  background = m_window.m_theme->Background();
        int       luminance  = (int) (((background >> 16) & 0xFF) * 299 + ((background >> 8) & 0xFF) * 587 + (background & 0xFF) * 114) / 1000;

        DxuiDwm::ApplyImmersiveDarkMode (m_window.GetHwnd(), luminance < 128);
    }

    m_window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerThemes::Preview
//
//  The theme on a row of the Theme menu, in the menu's order, applied while
//  the pointer is over it. A row past the end, or none, changes nothing.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerThemes::Preview (int index)
{
    if (index < 0 || index >= (int) std::size (s_kpszThemeRows) || m_window.m_prefs.theme == s_kpszThemeRows[index])
    {
        return;
    }

    Select (s_kpszThemeRows[index]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerThemes::Select
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerThemes::Select (const char * name)
{
    m_window.m_prefs.theme = name;
    Apply();

    //  A Casso theme brings its own CRT effects to a picture preview.
    m_window.m_previewPane.UpdateCrtPicture (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerThemes::IsCassoThemeName
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerThemes::IsCassoThemeName (const std::string & name)
{
    return name == CassoExplorerPrefs::kThemeSkeuomorphic
        || name == CassoExplorerPrefs::kThemeDarkModern
        || name == CassoExplorerPrefs::kThemeRetroTerminal;
}
