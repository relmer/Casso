#pragma once

#include "Pch.h"

#include "Ui/Chrome/CassoTheme.h"

class CassoExplorerWindow;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerThemes
//
//  The window's themes: Windows' light and dark, with the system's colors
//  and accent, and Casso's, the one in the preferences chosen and applied
//  to the window and its bars, and a Theme menu row previewed while the
//  pointer is over it.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerThemes
{
public:
    explicit ExplorerThemes (CassoExplorerWindow & window);

    void  ChooseInitial        ();
    void  Apply                ();
    void  AdoptSystemColors    ();
    void  HandleSystemThemeChanged ();

    //  The theme on a row of the Theme menu, in the menu's order; a theme by
    //  its preferences name, written to them.
    void  Preview (int index);
    void  Select  (const char * name);

    static bool  IsCassoThemeName (const std::string & name);

private:
    CassoExplorerWindow  & m_window;

    DxuiLightTheme         m_lightTheme;
    DxuiDarkTheme          m_darkTheme;
    CassoTheme             m_cassoTheme;
    bool                   m_applying = false;   // a theme being applied, which sends a theme change to the window
};