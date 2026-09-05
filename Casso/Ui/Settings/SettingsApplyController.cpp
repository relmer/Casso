#include "Pch.h"

#include "SettingsApplyController.h"

#include "SettingsApplyAdapter.h"

#include "SettingsMachineCatalog.h"
#include "SettingsPreviewController.h"

#include "../../EmulatorShell.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Bind
//
////////////////////////////////////////////////////////////////////////////////

void SettingsApplyController::Bind (
    SettingsPanelState     * state,
    UserConfigStore        * ucs,
    GlobalUserPrefs        * prefs,
    IFileSystem            * fs,
    EmulatorShell          * emuShell,
    std::function<void()>    onChromeThemeChanged,
    SettingsMachineCatalog * catalog)
{
    m_state                = state;
    m_ucs                  = ucs;
    m_prefs                = prefs;
    m_fs                   = fs;
    m_emuShell             = emuShell;
    m_onChromeThemeChanged = std::move (onChromeThemeChanged);
    m_catalog              = catalog;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SnapshotBaselines
//
//  Captures the current CRT block for every monitor type plus the
//  active monitor index, so Cancel can revert any live-preview edits
//  the user made -- including edits to monitors other than the one
//  active at panel open (they may have switched mid-edit).
//
////////////////////////////////////////////////////////////////////////////////

void SettingsApplyController::SnapshotBaselines()
{
    if (m_prefs != nullptr)
    {
        m_baselineCrt = m_prefs->crtOverrides;
    }

    if (m_state != nullptr)
    {
        m_baselineColorMode = (int) m_state->GetPrefs().colorMode;
    }
    else
    {
        m_baselineColorMode = -1;
    }

    // FR-132: remember the persisted active theme so a Cancel after an
    // "Apply now" live-apply can restore it. No theme has been live-
    // applied yet at Show time.
    m_baselineTheme    = (m_prefs != nullptr) ? m_prefs->activeTheme : std::string();
    m_themeAppliedLive = false;

    // Printing prefs baseline (global host-service prefs) for Cancel / save.
    if (m_prefs != nullptr)
    {
        m_baselinePrintOutputDpi   = m_prefs->printOutputDpi;
        m_baselinePrintDotStyle    = m_prefs->printDotStyle;
        m_baselinePrinterAudioEnabled     = m_prefs->printerAudioEnabled;
        m_baselinePrinterAudioVolume      = m_prefs->printerAudioVolume;
        m_baselinePrinterAudioPanOverride = m_prefs->printerAudioPanOverride;
        m_baselinePrinterAudioPan         = m_prefs->printerAudioPan;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClearPending
//
////////////////////////////////////////////////////////////////////////////////

void SettingsApplyController::ClearPending()
{
    m_pendingMachine.clear();
    m_pendingTheme.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  StagePendingMachine
//
////////////////////////////////////////////////////////////////////////////////

void SettingsApplyController::StagePendingMachine (const std::string & name)
{
    m_pendingMachine = name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StagePendingTheme
//
////////////////////////////////////////////////////////////////////////////////

void SettingsApplyController::StagePendingTheme (const std::string & name)
{
    m_pendingTheme = name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyThemeLive  (FR-132 -- "Apply now")
//
//  Reskins the real chrome to the given theme immediately without
//  persisting, and keeps the pick staged so a subsequent OK persists it.
//  Records that a live apply happened so Cancel restores the baseline
//  theme captured at Show.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsApplyController::ApplyThemeLive (const std::string & name)
{
    HRESULT  hr = S_OK;



    if (m_emuShell == nullptr || name.empty())
    {
        return;
    }

    hr = m_emuShell->ApplyThemeLive (name);

    IGNORE_RETURN_VALUE (hr, S_OK);
    if (m_onChromeThemeChanged)
    {
        m_onChromeThemeChanged();
    }

    m_pendingTheme     = name;   // OK still persists the chosen theme
    m_themeAppliedLive = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WillMachineChange  (FR-131)
//
//  True when a machine is staged that differs from the running machine,
//  so clicking OK (CommitApply -> DoMachineSelect) would power-cycle into
//  a different machine. Mirrors the guard CommitApply uses before it
//  triggers the switch.
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsApplyController::WillMachineChange() const
{
    std::wstring  current;
    std::string   currentNarrow;
    bool          changes = false;



    // Nothing staged means nothing to change. Machine ids are ASCII, so the
    // narrowing is a straight byte copy rather than a codepage conversion.
    if (!m_pendingMachine.empty() && m_emuShell != nullptr)
    {
        current = m_emuShell->GetCurrentMachineName();
        currentNarrow.reserve (current.size());

        for (wchar_t c : current)
        {
            currentNarrow.push_back ((char) (unsigned char) c);
        }

        changes = (m_pendingMachine != currentNarrow);
    }

    return changes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsResetRequired
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsApplyController::IsResetRequired() const
{
    return (m_state != nullptr) && m_state->RequiresReset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommitApply
//
//  Runs the full apply pipeline:
//    * push state changes through the live-effect adapter
//    * save the per-machine delta JSON
//    * diff + save the per-monitor CRT blocks
//    * re-snapshot baselines so a follow-up Cancel reverts to here
//    * activate any staged theme
//    * trigger a machine switch when one is pending OR when the apply
//      adapter queued a hardware reset
//
////////////////////////////////////////////////////////////////////////////////

void SettingsApplyController::CommitApply()
{
    JsonValue             currentJson;
    HRESULT               hr             = S_OK;
    bool                  savesRefused   = false;
    std::string           pendingMachine;
    std::wstring          currentMachine;
    std::string           currentMachineNarrow;



    SettingsApplyAdapter  adapter (*m_emuShell);



    if (m_state == nullptr || m_emuShell == nullptr)
    {
        return;
    }

    hr = m_state->Apply (adapter, currentJson);
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (m_ucs != nullptr && m_fs != nullptr && !m_state->GetMachineName().empty())
    {
        // BuildJson rooted at the merged JSON includes the canonical
        // version stamp; SaveDelta diffs against the embedded default
        // so only user-changed keys persist.
        hr = m_ucs->SaveDelta (m_state->GetMachineName(),
                                currentJson,
                                m_state->GetDefaultJson(),
                                *m_fs);

        if (FAILED (hr))
        {
            savesRefused = true;
        }

        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    pendingMachine.swap (m_pendingMachine);

    // CRT sliders were already mutating the active monitor block live;
    // CommitApply diffs ALL monitor blocks (the user may have edited
    // multiple before clicking OK) and saves on any change. Single
    // Save call covers every block since GlobalUserPrefs writes the
    // whole file atomically.
    if (m_prefs != nullptr)
    {
        bool  anyCrtChanged   = false;
        bool  anyPrintChanged = false;

        anyPrintChanged =
            m_prefs->printOutputDpi          != m_baselinePrintOutputDpi          ||
            m_prefs->printDotStyle           != m_baselinePrintDotStyle           ||
            m_prefs->printerAudioEnabled     != m_baselinePrinterAudioEnabled     ||
            m_prefs->printerAudioVolume      != m_baselinePrinterAudioVolume      ||
            m_prefs->printerAudioPanOverride != m_baselinePrinterAudioPanOverride ||
            m_prefs->printerAudioPan         != m_baselinePrinterAudioPan;

        // Presence counts as a change, not just a differing value: clearing
        // a pair's last override is an edit worth saving, and the map compare
        // sees the entry disappear where a field-by-field walk would not.
        anyCrtChanged = (m_prefs->crtOverrides != m_baselineCrt);

        if (anyCrtChanged || anyPrintChanged)
        {
            HRESULT  hrSave = S_OK;

            if (m_ucs != nullptr)
            {
                hrSave = m_ucs->SaveAll (*m_prefs, *m_fs);
            }
            else
            {
                hrSave = m_prefs->Save (m_emuShell->GetAssetBaseDir(), *m_fs);
            }

            if (FAILED (hrSave))
            {
                savesRefused = true;
            }

            IGNORE_RETURN_VALUE (hrSave, S_OK);
        }

        // Re-snapshot baselines so subsequent Cancel after another
        // round of edits reverts to THIS committed state, not the
        // pre-commit one.
        //
        // NOT when a save was refused, though. The baselines are what Cancel
        // reverts to, so advancing them to values that never reached disk
        // leaves the sheet with nothing to go back to and the next launch
        // showing settings the user watched take effect and then lose.
        if (!savesRefused)
        {
            m_baselineCrt = m_prefs->crtOverrides;

            m_baselinePrintOutputDpi   = m_prefs->printOutputDpi;
            m_baselinePrintDotStyle    = m_prefs->printDotStyle;
            m_baselinePrinterAudioEnabled     = m_prefs->printerAudioEnabled;
            m_baselinePrinterAudioVolume      = m_prefs->printerAudioVolume;
            m_baselinePrinterAudioPanOverride = m_prefs->printerAudioPanOverride;
            m_baselinePrinterAudioPan         = m_prefs->printerAudioPan;
        }
    }

    // Every save on this path is fire-and-forget by design, so a refusal would
    // otherwise close the sheet looking like it worked. The file is intact and
    // the startup message already explained why, but that was minutes ago and
    // said nothing about the change the user just made.
    //
    // POSTED, not shown. This runs inside SettingsSheet::OnOk, which has not
    // returned and so has not closed the sheet; a modal opened here would run
    // a nested message loop against a sheet still mid-commit.
    if (savesRefused)
    {
        m_emuShell->PostNotification (
            L"Settings not saved\n\n"
            L"Casso could not read your settings file, so these changes were "
            L"applied to this session but not written to disk. Repair or move "
            L"the file, then restart Casso.");
    }

    m_baselineColorMode = (int) m_state->GetPrefs().colorMode;

    // Apply the staged theme BEFORE any machine switch so the chrome
    // is already in its final geometry when SwitchMachine triggers a
    // resize / repaint cascade. Theme apply is idempotent when the
    // staged value matches the active theme, so the typical no-change
    // path costs nothing.
    if (!m_pendingTheme.empty())
    {
        HRESULT  hrTheme = m_emuShell->ApplyAndPersistTheme (m_pendingTheme);

        IGNORE_RETURN_VALUE (hrTheme, S_OK);
        if (m_onChromeThemeChanged)
        {
            m_onChromeThemeChanged();
        }

        m_pendingTheme.clear();
    }

    // FR-132: the committed theme becomes the new baseline so a later
    // Cancel (after another "Apply now") reverts to THIS state, and the
    // live-apply flag is cleared now that the pick is persisted.
    m_baselineTheme    = (m_prefs != nullptr) ? m_prefs->activeTheme : m_baselineTheme;
    m_themeAppliedLive = false;

    currentMachine = m_emuShell->GetCurrentMachineName();
    currentMachineNarrow.reserve (currentMachine.size());
    for (wchar_t c : currentMachine)
    {
        currentMachineNarrow.push_back ((char) (unsigned char) c);
    }

    // m_catalog->DoMachineSelect handles the ROM bootstrap modal +
    // posts the SwitchMachine command to the CPU thread. Either an
    // explicit machine change OR a hardware-reset-requiring edit
    // drives a full switch; pendingMachine wins because it's the
    // user's explicit choice.
    if (m_catalog != nullptr)
    {
        if (!pendingMachine.empty() && pendingMachine != currentMachineNarrow)
        {
            m_catalog->DoMachineSelect (pendingMachine);
        }
        else if (adapter.IsResetQueued() && !currentMachineNarrow.empty())
        {
            m_catalog->DoMachineSelect (currentMachineNarrow);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Cancel
//
//  Drops staged picks, rolls back live-preview CRT edits across every
//  monitor block, restores the prior color mode in the shell, and
//  resets the preview state machine. Callers handle panel-visibility
//  themselves.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsApplyController::Cancel (SettingsPreviewController & preview)
{
    m_pendingMachine.clear();
    m_pendingTheme.clear();

    // Roll back live-preview edits across every monitor block. The
    // shader picks the restored values up on the next frame via the
    // per-frame MakeCrtParams path.
    if (m_prefs != nullptr)
    {
        m_prefs->crtOverrides = m_baselineCrt;

        // Revert Printing edits (no live effect; they only bind at the next
        // delivery / printer sound, so this simply un-does the staged writes).
        m_prefs->printOutputDpi   = m_baselinePrintOutputDpi;
        m_prefs->printDotStyle    = m_baselinePrintDotStyle;
        m_prefs->printerAudioEnabled     = m_baselinePrinterAudioEnabled;
        m_prefs->printerAudioVolume      = m_baselinePrinterAudioVolume;
        m_prefs->printerAudioPanOverride = m_baselinePrinterAudioPanOverride;
        m_prefs->printerAudioPan         = m_baselinePrinterAudioPan;
    }

    if (m_emuShell != nullptr && m_baselineColorMode >= 0)
    {
        m_emuShell->SetColorModeLive (m_baselineColorMode);
    }

    // FR-132: undo an "Apply now" live theme apply by re-activating the
    // theme that was active at panel open. No persist happened, so this
    // just reskins the chrome back.
    if (m_themeAppliedLive && m_emuShell != nullptr)
    {
        HRESULT  hrTheme = m_emuShell->ApplyThemeLive (m_baselineTheme);

        IGNORE_RETURN_VALUE (hrTheme, S_OK);
        if (m_onChromeThemeChanged)
        {
            m_onChromeThemeChanged();
        }
    }

    m_themeAppliedLive = false;

    preview.Reset();

    if (m_state != nullptr)
    {
        m_state->Cancel();
    }
}
