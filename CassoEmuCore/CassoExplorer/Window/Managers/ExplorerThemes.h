#pragma once

#include "Pch.h"

#include "Ui/Chrome/CassoTheme.h"

struct ExplorerContext;





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
    explicit ExplorerThemes (ExplorerContext & context);

    void         ChooseInitial     ();
    void         Apply             ();
    void         AdoptSystemColors ();
    void         Preview           (int index);
    void         Select            (const char * name);
    static bool  IsCassoThemeName  (const std::string & name);

    void  HandleSystemThemeChanged ();

private:
    ExplorerContext  & m_context;
    DxuiLightTheme     m_lightTheme;
    DxuiDarkTheme      m_darkTheme;
    CassoTheme         m_cassoTheme;
    bool               m_applying   = false;   // a theme being applied, which sends a theme change to the window
};