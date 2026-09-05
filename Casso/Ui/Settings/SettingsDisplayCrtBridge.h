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
//  user's stored overrides (GlobalUserPrefs::crtOverrides) -- into the
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
    // mode the page is showing. ResolveWithoutUser answers what the field
    // would show if the user had never touched it.
    std::string   ActiveOverrideKey  () const;
    CrtResolved   ResolveActive      () const;
    CrtResolved   ResolveWithoutUser () const;

    // Record one field as the user's. Never seeds a sibling: that is the
    // whole difference from the flag this replaced, which snapshotted all
    // eleven values before it latched.
    //
    // Setting a field to the value it already resolves to is NOT an
    // adjustment, so it clears the override instead of storing one. Without
    // that, dragging a slider away and back would leave the field pinned
    // against every later theme change, and the user has no way to tell that
    // state apart from having never touched it. A pair left holding nothing
    // is erased so an empty entry never reaches the file.
    template <typename T>
    void  SetOverride (std::optional<T> CrtOverrides::* slot,
                       T CrtValues::*                   valueSlot,
                       T                                value)
    {
        std::string  key;



        if (m_prefs != nullptr)
        {
            key = ActiveOverrideKey();

            if (ResolveWithoutUser().values.*valueSlot == value)
            {
                auto  found = m_prefs->crtOverrides.find (key);

                if (found != m_prefs->crtOverrides.end())
                {
                    (found->second.*slot).reset();

                    if (found->second.IsEmpty())
                    {
                        m_prefs->crtOverrides.erase (found);
                    }
                }
            }
            else
            {
                m_prefs->crtOverrides[key].*slot = value;
            }

            // A badge reports the tier that supplied the value, so it goes
            // stale the instant this map changes. Republishing here is what
            // drops a row's badge as a control moves off its default and
            // brings the badge back when the control returns.
            PublishDefaultsHint();
        }
    }

    // A theme carries CRT defaults, so adopting a theme adopts them --
    // on Apply now, on OK, and on the Cancel that puts the old theme
    // back. Called from the sheet's chrome-theme-changed hook, which
    // every one of those paths already raises.
    void  AdoptThemeDefaults      ();

    // Installs the slider / toggle / monitor / restore-defaults
    // callbacks on the bound DisplayPage. The lambdas funnel through
    // SetOverride, so each one records only the field it changed and the
    // CRT shader picks the edit up on the next frame.
    void  WireDisplayPageCallbacks ();


private:
    GlobalUserPrefs     * m_prefs       = nullptr;
    ThemeManager        * m_themes      = nullptr;
    SettingsPanelState  * m_state       = nullptr;
    DisplayPage         * m_displayPage = nullptr;
    EmulatorShell       * m_emuShell    = nullptr;
};
