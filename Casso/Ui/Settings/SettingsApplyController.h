#pragma once

#include "Pch.h"

#include "SettingsPanelState.h"

#include "../../Config/GlobalUserPrefs.h"


class UserConfigStore;
class IFileSystem;
class EmulatorShell;
class SettingsWindow;
class SettingsMachineCatalog;
class SettingsPreviewController;





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsApplyController
//
//  Owns the Apply / Cancel pipeline for the Settings panel: dirty-
//  state tracking, the per-monitor CRT baseline snapshot (so Cancel
//  can revert any live-preview edits), staged Machine / Theme picks,
//  the SaveDelta write, the GlobalUserPrefs save, and the post-commit
//  theme apply + machine switch.
//
//  Construct empty, then Bind() once the panel's dependency pointers
//  are wired during Initialize. The panel forwards Apply / Cancel
//  button clicks and stages user picks via the Stage*() setters;
//  the controller does the rest.
//
////////////////////////////////////////////////////////////////////////////////

class SettingsApplyController
{
public:
    void  Bind (SettingsPanelState     * state,
                UserConfigStore        * ucs,
                GlobalUserPrefs        * prefs,
                IFileSystem            * fs,
                EmulatorShell          * emuShell,
                SettingsWindow         * window,
                SettingsMachineCatalog * catalog);

    void  SnapshotBaselines   ();
    void  ClearPending        ();
    void  StagePendingMachine (const std::string & name);
    void  StagePendingTheme   (const std::string & name);

    bool  IsResetRequired     () const;
    void  CommitApply         ();
    void  Cancel              (SettingsPreviewController & preview);


private:
    SettingsPanelState     * m_state    = nullptr;
    UserConfigStore        * m_ucs      = nullptr;
    GlobalUserPrefs        * m_prefs    = nullptr;
    IFileSystem            * m_fs       = nullptr;
    EmulatorShell          * m_emuShell = nullptr;
    SettingsWindow         * m_window   = nullptr;
    SettingsMachineCatalog * m_catalog  = nullptr;

    // Staged user picks. Live writes occur on commit; cancel just
    // clears these without touching the running machine / theme.
    std::string  m_pendingMachine;
    std::string  m_pendingTheme;

    // Baseline captures the value at Show time so Cancel can revert.
    // The per-monitor schema means we keep a snapshot of ALL 4 mode
    // blocks because the user can switch monitors inside the panel and
    // edit multiple blocks before they decide whether to commit.
    // m_baselineColorMode tracks which monitor was active at open.
    GlobalUserPrefs::Crt  m_baselineCrt[GlobalUserPrefs::kCrtModeCount] = {};
    int                   m_baselineColorMode = -1;
};
