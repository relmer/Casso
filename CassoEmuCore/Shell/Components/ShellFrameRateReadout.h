#pragma once

#include "Pch.h"

class DxuiPanel;
class EmulatorShell;
class ShellChrome;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellFrameRateReadout
//
//  The frames-per-second readout: shadowed text in a corner rather than a
//  notice's centered band. Adopted after the drive row, and updated from the
//  prefs in the per-frame UI upkeep.
//
////////////////////////////////////////////////////////////////////////////////

class ShellFrameRateReadout
{
public:
    ShellFrameRateReadout (EmulatorShell & shell, ShellChrome & chrome) : m_shell (shell), m_chrome (chrome) {}

    void  AdoptFrameRateReadout (DxuiPanel & root);
    void  SyncFrameRateReadout  ();

private:
    friend class ShellChrome;

    EmulatorShell     & m_shell;
    ShellChrome       & m_chrome;
    DxuiShadowedText    m_fpsReadout;
};
