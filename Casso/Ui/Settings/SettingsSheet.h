#pragma once

#include "Pch.h"

#include "Window/DxuiPropertySheet.h"

#include "SettingsPanelState.h"
#include "SettingsMachineCatalog.h"
#include "SettingsDisplayCrtBridge.h"
#include "MachinePage.h"
#include "HardwarePage.h"
#include "ThemePage.h"
#include "DisplayPage.h"


class UserConfigStore;
struct GlobalUserPrefs;
class ThemeManager;
class EmulatorShell;
class IFileSystem;




////////////////////////////////////////////////////////////////////////////////
//
//  SettingsSheet
//
//  T162 slice 3: the DxuiPropertySheet-based replacement for the bespoke
//  SettingsPanel + SettingsWindow pair. Hosts the four DxuiPropertyPage
//  setting pages behind the framework's tab strip + (Apply-hidden)
//  OK / Cancel row.
//
//  Built in parallel with the legacy path: slice 3a stands up the pages
//  and shows the sheet modally from a temporary dev trigger to prove the
//  structure. The cross-cutting commit (OnOk / OnCancel), the modeless
//  presentation, and the blur / live-preview compositor land in later
//  slices; the swap onto IDM_VIEW_SETTINGS and deletion of the legacy
//  types happens in 3d.
//
////////////////////////////////////////////////////////////////////////////////

class SettingsSheet : public DxuiPropertySheet
{
public:
    SettingsSheet  () = default;
    ~SettingsSheet () override = default;

    //
    //  Wire dependencies, create the window (which builds + populates the
    //  pages), and show it modally. Returns when the dialog closes.
    //  (Slice 3a is modal for verification; 3d makes it modeless per
    //  FR-041 so the emulator keeps running.)
    //
    HRESULT OpenModal (HINSTANCE         hInstance,
                       HWND              ownerHwnd,
                       UserConfigStore & ucs,
                       GlobalUserPrefs & prefs,
                       ThemeManager    & themes,
                       EmulatorShell   & emuShell,
                       IFileSystem     & fs);

protected:
    void  OnBuildPages () override;

private:
    // Drive-sound audition for the Machine page's play (>) buttons. Ported
    // verbatim from SettingsPanel: push the current volumes / pan / mechanism
    // to the engine and post the one-shot test command. Self-contained (needs
    // only the emulator shell + staged prefs); the cancel-revert bookkeeping
    // lands with the commit controller in a later slice.
    void  AuditionDriveSound    (int drive, int kind, bool centered);
    void  PushDriveAudioToEngine (float motor, float head, float door,
                                  float pan0, float pan1,
                                  const std::string & mechanism);

    UserConfigStore * m_ucs      = nullptr;
    GlobalUserPrefs * m_prefs    = nullptr;
    ThemeManager    * m_themes   = nullptr;
    EmulatorShell   * m_emuShell = nullptr;
    IFileSystem     * m_fs       = nullptr;

    SettingsPanelState        m_state;
    SettingsMachineCatalog    m_catalog;
    SettingsDisplayCrtBridge  m_crt;

    // Last mechanism pushed to the engine, so PushDriveAudioToEngine skips a
    // redundant WAV reload when it hasn't changed.
    std::string               m_lastAuditionMechanism;

    // Owned by the DxuiPropertySheet child list (CreatePage); raw pointers
    // for wiring only.
    MachinePage  * m_machinePage  = nullptr;
    HardwarePage * m_hardwarePage = nullptr;
    ThemePage    * m_themePage    = nullptr;
    DisplayPage  * m_displayPage  = nullptr;
};
