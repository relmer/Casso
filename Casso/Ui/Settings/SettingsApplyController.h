#pragma once

#include "Pch.h"

#include "SettingsPanelState.h"

#include "../../Config/GlobalUserPrefs.h"



class UserConfigStore;
class IFileSystem;
class EmulatorShell;
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
    //  onChromeThemeChanged fires after a live / persisted theme apply so the
    //  host window can re-skin its own chrome. Decoupled from the concrete
    //  window type (SettingsWindow vs DxuiPropertySheet) so both hosts share
    //  this controller.
    void  Bind (SettingsPanelState     * state,
                UserConfigStore        * ucs,
                GlobalUserPrefs        * prefs,
                IFileSystem            * fs,
                EmulatorShell          * emuShell,
                std::function<void()>    onChromeThemeChanged,
                SettingsMachineCatalog * catalog);

    void  SnapshotBaselines   ();
    void  ClearPending        ();
    void  StagePendingMachine (const std::string & name);
    void  StagePendingTheme   (const std::string & name);

    // FR-132: apply the given theme LIVE to the real chrome now, without
    // closing the panel and without persisting. Keeps the pick staged so
    // OK still persists it; a follow-up Cancel re-activates the baseline
    // theme captured at panel open.
    void  ApplyThemeLive      (const std::string & name);

    // FR-131: true iff a machine switch is staged that differs from the
    // running machine (so OK would restart into a different machine).
    bool  WillMachineChange   () const;
    const std::string & PendingMachine () const { return m_pendingMachine; }

    bool  IsResetRequired     () const;
    void  CommitApply         ();
    void  Cancel              (SettingsPreviewController & preview);


private:
    SettingsPanelState     * m_state    = nullptr;
    UserConfigStore        * m_ucs      = nullptr;
    GlobalUserPrefs        * m_prefs    = nullptr;
    IFileSystem            * m_fs       = nullptr;
    EmulatorShell          * m_emuShell = nullptr;
    std::function<void()>    m_onChromeThemeChanged;
    SettingsMachineCatalog * m_catalog  = nullptr;

    // Staged user picks. Live writes occur on commit; cancel just
    // clears these without touching the running machine / theme.
    std::string  m_pendingMachine;
    std::string  m_pendingTheme;

    // FR-132: theme baseline captured at Show (the persisted active
    // theme id) plus a flag tracking whether "Apply now" live-applied a
    // theme, so Cancel knows to re-activate the baseline.
    std::string  m_baselineTheme;
    bool         m_themeAppliedLive = false;

    // Baseline captures the value at Show time so Cancel can revert.
    // The per-monitor schema means we keep a snapshot of ALL 4 mode
    // blocks because the user can switch monitors inside the panel and
    // edit multiple blocks before they decide whether to commit.
    // m_baselineColorMode tracks which monitor was active at open.
    GlobalUserPrefs::Crt  m_baselineCrt[GlobalUserPrefs::kCrtModeCount] = {};
    int                   m_baselineColorMode = -1;
};
