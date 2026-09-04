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

    int   GetActiveModeIdx        () const;
    void  ReseedFromActiveMode    ();
    void  PublishDefaultsHint     ();
    void  ResetActiveToDefaults   ();

    // The override key and the resolved picture for whichever monitor and
    // mode the page is showing.
    std::string   ActiveOverrideKey () const;
    CrtResolved   ResolveActive     () const;

    // Record one field as the user's. Never seeds a sibling: that is the
    // whole difference from the flag this replaced, which snapshotted all
    // eleven values before it latched.
    template <typename T>
    void  SetOverride (std::optional<T> CrtOverrides::* member, T value)
    {
        if (m_prefs != nullptr)
        {
            m_prefs->crtOverrides[ActiveOverrideKey()].*member = value;
        }
    }

    // A theme carries CRT defaults, so adopting a theme adopts them --
    // on Apply now, on OK, and on the Cancel that puts the old theme
    // back. Called from the sheet's chrome-theme-changed hook, which
    // every one of those paths already raises.
    void  AdoptThemeDefaults      ();

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
