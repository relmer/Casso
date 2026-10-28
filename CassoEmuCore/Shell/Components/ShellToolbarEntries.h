#pragma once

#include "Pch.h"

#include "Ui/Chrome/VolumeFlyout.h"

class EmulatorShell;
class PrinterStatusLed;
class ShellChrome;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellToolbarEntries
//
//  The command toolbar's entries: built from the command table the menu
//  bar reads, the volume flyout's sink and starting level, the theme and
//  monitor color pickers, and each entry's state updated to match the
//  emulator's. The toolbar itself is the chrome's.
//
////////////////////////////////////////////////////////////////////////////////

class ShellToolbarEntries
{
public:
    ShellToolbarEntries (EmulatorShell & shell, ShellChrome & chrome) : m_shell (shell), m_chrome (chrome) {}

    void  BuildToolbar            (PrinterStatusLed & printerLed);
    void  WireToolbarPickers      ();
    void  RefreshToolbarThemeList ();
    void  SyncToolbarState        ();

private:
    EmulatorShell              & m_shell;
    ShellChrome                & m_chrome;
    VolumeFlyout                 m_volumeFlyout;

    // Theme ids in the toolbar picker's row order, so a picked row resolves
    // to the id ThemeManager wants. Rebuilt whenever the catalog is.
    std::vector<std::string>     m_toolbarThemeIds;
};
