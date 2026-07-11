#include "Pch.h"

#include "SettingsApplyController.h"

#include "SettingsMachineCatalog.h"
#include "SettingsPreviewController.h"

#include "../../EmulatorShell.h"
#include "../../Config/UserConfigStore.h"
#include "../../Config/IFileSystem.h"

#include "resource.h"


namespace
{
    ////////////////////////////////////////////////////////////////////////////
    //
    //  SettingsApplyAdapter
    //
    //  Bridges the pure-logic ISettingsApplySink contract into the
    //  EmulatorShell command queue. Live-effect fields post commands so
    //  the audio mixer / CRT pipeline picks them up on the next CPU
    //  tick; QueueMachineReset is recorded and consumed by the modal
    //  confirm path in SettingsPanel.
    //
    ////////////////////////////////////////////////////////////////////////////

    class SettingsApplyAdapter : public ISettingsApplySink
    {
    public:
        explicit SettingsApplyAdapter (EmulatorShell & shell)
            : m_shell (shell)
        {
        }

        void ApplySpeedMode (SettingsSpeedMode mode) override
        {
            WORD  id = IDM_MACHINE_SPEED_1X;

            switch (mode)
            {
                case SettingsSpeedMode::Authentic: id = IDM_MACHINE_SPEED_1X;  break;
                case SettingsSpeedMode::Double:    id = IDM_MACHINE_SPEED_2X;  break;
                case SettingsSpeedMode::Maximum:   id = IDM_MACHINE_SPEED_MAX; break;
            }
            PostMessageW (m_shell.GetHwnd(), WM_COMMAND, MAKEWPARAM (id, 0), 0);
        }

        void ApplyColorMode (SettingsColorMode mode) override
        {
            WORD  id = IDM_VIEW_COLOR;

            switch (mode)
            {
                case SettingsColorMode::Color: id = IDM_VIEW_COLOR; break;
                case SettingsColorMode::Green: id = IDM_VIEW_GREEN; break;
                case SettingsColorMode::Amber: id = IDM_VIEW_AMBER; break;
                case SettingsColorMode::White: id = IDM_VIEW_WHITE; break;
            }
            PostMessageW (m_shell.GetHwnd(), WM_COMMAND, MAKEWPARAM (id, 0), 0);
        }

        void ApplyFloppySound  (bool enabled) override
        {
            m_shell.PostCommand (enabled ? IDM_AUDIO_DRIVE_ENABLE
                                         : IDM_AUDIO_DRIVE_DISABLE);
        }

        void ApplyMechanism    (const std::string & mechanism) override
        {
            m_shell.PostCommand (IDM_AUDIO_DRIVE_MECHANISM, mechanism);
        }

        void ApplyDriveVolumes (float motor, float head, float door) override
        {
            char  payload[32] = {};

            sprintf_s (payload, "%d,%d,%d",
                       (int) std::lround (motor * 100.0f),
                       (int) std::lround (head  * 100.0f),
                       (int) std::lround (door  * 100.0f));
            m_shell.PostCommand (IDM_AUDIO_DRIVE_VOLUMES, payload);
        }

        void ApplyDrivePan     (float driveOnePan, float driveTwoPan) override
        {
            char  payload[32] = {};

            sprintf_s (payload, "%d,%d",
                       (int) std::lround (driveOnePan * 100.0f),
                       (int) std::lround (driveTwoPan * 100.0f));
            m_shell.PostCommand (IDM_AUDIO_DRIVE_PAN, payload);
        }

        void ApplyWriteProtect (int drive, bool wp)            override { UNREFERENCED_PARAMETER (drive); UNREFERENCED_PARAMETER (wp); }

        void ApplyExternalDriveConnected (bool connected) override
        {
            // //c-only live effect: reveal/hide the second drive-mount widget.
            // Non-//c machines ignore the command (their second drive is fixed
            // hardware). Cheap + idempotent, so pushed on every Apply. Routed
            // via PostMessage(WM_COMMAND) -- NOT the CPU command queue -- so it
            // runs on the UI thread: it relays the chrome (menu bar + drive
            // band), which asserts UI-thread affinity. Mirrors ApplyColorMode.
            WORD  id = connected ? IDM_DRIVE_EXTERNAL_CONNECT
                                 : IDM_DRIVE_EXTERNAL_DISCONNECT;
            PostMessageW (m_shell.GetHwnd(), WM_COMMAND, MAKEWPARAM (id, 0), 0);
        }

        void ApplyMouseConnected (bool connected) override
        {
            // //c-only live effect (FR-013b): connect/disconnect the mouse
            // peripheral. UI-thread routed like the external drive.
            WORD  id = connected ? IDM_MOUSE_CONNECT : IDM_MOUSE_DISCONNECT;
            PostMessageW (m_shell.GetHwnd(), WM_COMMAND, MAKEWPARAM (id, 0), 0);
        }

        void QueueMachineReset ()                              override { m_resetQueued = true; }

        bool  ResetQueued () const { return m_resetQueued; }

    private:
        EmulatorShell & m_shell;
        bool            m_resetQueued = false;
    };
}





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

void SettingsApplyController::SnapshotBaselines ()
{
    if (m_prefs != nullptr)
    {
        for (size_t i = 0; i < GlobalUserPrefs::kCrtModeCount; i++)
        {
            m_baselineCrt[i] = m_prefs->crtByMode[i];
        }
    }
    if (m_state != nullptr)
    {
        m_baselineColorMode = (int) m_state->Prefs().colorMode;
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
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClearPending
//
////////////////////////////////////////////////////////////////////////////////

void SettingsApplyController::ClearPending ()
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
    if (m_emuShell == nullptr || name.empty())
    {
        return;
    }

    HRESULT  hr = m_emuShell->ApplyThemeLive (name);

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

bool SettingsApplyController::WillMachineChange () const
{
    std::wstring  current;
    std::string   currentNarrow;


    if (m_pendingMachine.empty() || m_emuShell == nullptr)
    {
        return false;
    }

    current = m_emuShell->CurrentMachineName();
    currentNarrow.reserve (current.size());
    for (wchar_t c : current)
    {
        currentNarrow.push_back ((char) (unsigned char) c);
    }

    return m_pendingMachine != currentNarrow;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsResetRequired
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsApplyController::IsResetRequired () const
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

void SettingsApplyController::CommitApply ()
{
    SettingsApplyAdapter  adapter (*m_emuShell);
    JsonValue             currentJson;
    HRESULT               hr             = S_OK;
    std::string           pendingMachine;
    std::wstring          currentMachine;
    std::string           currentMachineNarrow;



    if (m_state == nullptr || m_emuShell == nullptr)
    {
        return;
    }

    hr = m_state->Apply (adapter, currentJson);
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (m_ucs != nullptr && m_fs != nullptr && !m_state->MachineName().empty())
    {
        // BuildJson rooted at the merged JSON includes the canonical
        // version stamp; SaveDelta diffs against the embedded default
        // so only user-changed keys persist.
        hr = m_ucs->SaveDelta (m_state->MachineName(),
                                currentJson,
                                m_state->DefaultJson(),
                                *m_fs);
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
        bool    anyCrtChanged = false;
        size_t  i             = 0;

        for (i = 0; i < GlobalUserPrefs::kCrtModeCount; i++)
        {
            const auto &  cur  = m_prefs->crtByMode[i];
            const auto &  base = m_baselineCrt[i];

            if (cur.brightness         != base.brightness         ||
                cur.contrast           != base.contrast           ||
                cur.gamma              != base.gamma              ||
                cur.persistence        != base.persistence        ||
                cur.scanlinesEnabled   != base.scanlinesEnabled   ||
                cur.scanlinesIntensity != base.scanlinesIntensity ||
                cur.bloomEnabled       != base.bloomEnabled       ||
                cur.bloomRadius        != base.bloomRadius        ||
                cur.bloomStrength      != base.bloomStrength      ||
                cur.colorBleedEnabled  != base.colorBleedEnabled  ||
                cur.colorBleedWidth    != base.colorBleedWidth    ||
                cur.userOverride       != base.userOverride)
            {
                anyCrtChanged = true;
            }
        }

        if (anyCrtChanged)
        {
            HRESULT  hrSave = S_OK;

            if (m_ucs != nullptr)
            {
                hrSave = m_ucs->SaveAll (*m_prefs, *m_fs);
            }
            else
            {
                hrSave = m_prefs->Save (m_emuShell->AssetBaseDir(), *m_fs);
            }
            IGNORE_RETURN_VALUE (hrSave, S_OK);
        }

        // Re-snapshot baselines so subsequent Cancel after another
        // round of edits reverts to THIS committed state, not the
        // pre-commit one.
        for (i = 0; i < GlobalUserPrefs::kCrtModeCount; i++)
        {
            m_baselineCrt[i] = m_prefs->crtByMode[i];
        }
    }
    m_baselineColorMode = (int) m_state->Prefs().colorMode;

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

    currentMachine = m_emuShell->CurrentMachineName();
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
            (void) m_catalog->DoMachineSelect (pendingMachine);
        }
        else if (adapter.ResetQueued() && !currentMachineNarrow.empty())
        {
            (void) m_catalog->DoMachineSelect (currentMachineNarrow);
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
        for (size_t i = 0; i < GlobalUserPrefs::kCrtModeCount; i++)
        {
            m_prefs->crtByMode[i] = m_baselineCrt[i];
        }
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
