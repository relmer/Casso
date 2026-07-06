#pragma once

#include "Pch.h"

#include "SettingsPanelState.h"


class EmulatorShell;
class UserConfigStore;
class IFileSystem;
class ThemeManager;
class HardwarePage;
class ThemePage;
struct GlobalUserPrefs;





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsMachineCatalog
//
//  Off-disk Machine + Theme catalog plumbing for the Settings panel.
//  Loads the active machine's JSON into SettingsPanelState, populates
//  the Machine / Theme dropdown widgets, and -- on commit -- runs the
//  asset bootstrap flow (ROM + Disk II audio download) before posting
//  the IDM_FILE_OPEN switch-machine command back to the emulator
//  shell.
//
//  All file / asset I/O lives here so SettingsPanel can stay focused
//  on widget plumbing and apply / cancel orchestration. Construct
//  empty, then Bind() once the panel's dependency pointers are wired
//  during Initialize.
//
////////////////////////////////////////////////////////////////////////////////

class SettingsMachineCatalog
{
public:
    void  Bind (EmulatorShell      * emuShell,
                UserConfigStore    * ucs,
                GlobalUserPrefs    * prefs,
                IFileSystem        * fs,
                ThemeManager       * themes,
                SettingsPanelState * state,
                HardwarePage       * machinePage,
                ThemePage          * themePage);

    void  LoadCurrentMachineIntoState ();
    void  PopulateMachineList         ();
    void  PopulateThemeList           ();

    // Runs the asset bootstrap flow then posts IDM_FILE_OPEN with the
    // selected machine name. Returns false if the user cancelled the
    // bootstrap dialog (caller should leave the active machine alone).
    bool  DoMachineSelect             (const std::string & machineName);


private:
    EmulatorShell      * m_emuShell    = nullptr;
    UserConfigStore    * m_ucs         = nullptr;
    GlobalUserPrefs    * m_prefs       = nullptr;
    IFileSystem        * m_fs          = nullptr;
    ThemeManager       * m_themes      = nullptr;
    SettingsPanelState * m_state       = nullptr;
    HardwarePage       * m_machinePage = nullptr;
    ThemePage          * m_themePage   = nullptr;
};
