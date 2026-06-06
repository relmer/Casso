#pragma once

#include "Pch.h"

#include "SettingsPanelState.h"

#include "../../Config/GlobalUserPrefs.h"


class ThemeManager;
class DisplayPage;
class EmulatorShell;





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsDisplayCrtBridge
//
//  Per-monitor CRT plumbing for the Settings panel's Display page.
//  Bridges three sources of truth -- monitor presets (CrtPresets),
//  active theme overrides (ThemeManager::GetActiveTheme), and the
//  user's stored overrides (GlobalUserPrefs::crtByMode) -- into the
//  slider widget state on DisplayPage.
//
//  Stateless across calls (everything reads through the bound refs);
//  the panel constructs the bridge once and Bind()s it after its own
//  dependency pointers have been wired during Initialize.
//
////////////////////////////////////////////////////////////////////////////////

class SettingsDisplayCrtBridge
{
public:
    void  Bind (GlobalUserPrefs    * prefs,
                ThemeManager       * themes,
                SettingsPanelState * state,
                DisplayPage        * displayPage,
                EmulatorShell      * emuShell);

    int   ActiveModeIdx           () const;
    void  ReseedFromActiveMode    ();
    void  PublishDefaultsHint     ();
    void  PromoteActiveToOverride ();
    void  ResetActiveToDefaults   ();

    // Installs the slider / toggle / monitor / restore-defaults
    // callbacks on the bound DisplayPage. The lambdas funnel through
    // PromoteActiveToOverride + the per-monitor crtByMode block so the
    // CRT shader picks live edits up on the next frame.
    void  WireDisplayPageCallbacks ();


private:
    GlobalUserPrefs     * m_prefs       = nullptr;
    ThemeManager        * m_themes      = nullptr;
    SettingsPanelState  * m_state       = nullptr;
    DisplayPage         * m_displayPage = nullptr;
    EmulatorShell       * m_emuShell    = nullptr;
};
