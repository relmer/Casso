#pragma once

#include "Pch.h"

#include "SettingsPanelState.h"

#include "Core/MachineScanner.h"




class UiShell;
class UserConfigStore;
struct GlobalUserPrefs;
class ThemeManager;
class EmulatorShell;
class IFileSystem;





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsPanel
//
//  Owner of the consolidated settings surface. The painter-driven view
//  layer is reintroduced in a later phase; in this baseline the panel
//  is a compiling skeleton that records dependencies and answers
//  IsVisible() consistently. Every entry point returns S_OK so the
//  command path stays exercised by callers (View → Settings…).
//
////////////////////////////////////////////////////////////////////////////////

class SettingsPanel
{
public:

    SettingsPanel  ();
    ~SettingsPanel ();

    HRESULT Initialize (UiShell         & uiShell,
                        UserConfigStore & ucs,
                        GlobalUserPrefs & prefs,
                        ThemeManager    & themes,
                        EmulatorShell   & emuShell,
                        IFileSystem     & fs);

    HRESULT                  Show      ();
    void                     Hide      ();
    bool                     IsVisible () const { return m_visible; }

    const SettingsPanelState & State () const { return m_state; }

    static const char * SpeedRadioValue (SettingsSpeedMode m);
    static const char * ColorRadioValue (SettingsColorMode m);

private:
    UiShell         * m_uiShell   = nullptr;
    UserConfigStore * m_ucs       = nullptr;
    GlobalUserPrefs * m_prefs     = nullptr;
    ThemeManager    * m_themes    = nullptr;
    EmulatorShell   * m_emuShell  = nullptr;
    IFileSystem     * m_fs        = nullptr;

    SettingsPanelState  m_state;
    bool                m_visible = false;
};
