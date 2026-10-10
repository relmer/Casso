#pragma once

#include "Pch.h"

#include "Config/GlobalUserPrefs.h"
#include "Config/Win32FileSystem.h"



class EmulatorShell;
class JsonValue;
class SettingsSheet;
class ThemeManager;
class UserConfigStore;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellSettings
//
//  The preferences and the Settings dialog: the global preferences and the
//  store that keeps them and each machine's own, the UI thread's file
//  system they are read and written through, the theme catalog, and the
//  modeless Settings sheet. What a preference DOES when it changes is the
//  business of whichever part of the emulator it configures; this is where
//  it is kept, saved and edited.
//
////////////////////////////////////////////////////////////////////////////////

class ShellSettings
{
public:
    explicit ShellSettings (EmulatorShell & shell);
    ~ShellSettings();

    GlobalUserPrefs  & GetPrefs        ()       { return m_globalPrefs; }
    Win32FileSystem  & GetFileSystem   ()       { return m_uiFs; }

    // Null until the asset base directory is known (the store) and until the
    // chrome is wired (the themes).
    UserConfigStore  * GetConfigStore  () const { return m_userConfigStore.get(); }
    ThemeManager     * GetThemeManager () const { return m_themeManager.get(); }

    void  CreateConfigStore  (const std::wstring & assetBaseDir);
    void  CreateThemeManager (const std::wstring & themesDir);

    // Flushes the in-memory GlobalUserPrefs to UserPrefs.json. Also the
    // WindowManager save callback, so per-monitor window placement edits land
    // on disk immediately after the user moves/resizes the window. Safe to
    // call before the store exists; it does nothing then.
    void  SaveGlobalPrefs ();

    // Marks GlobalUserPrefs dirty and arms the coalescing timer instead of
    // writing now. For a control that reports every intermediate value --
    // the volume slider fires on each drag tick -- where a write per tick
    // would put a file rewrite in the middle of a drag. The pending write
    // is flushed by the timer, by any SaveGlobalPrefs that beats it, and on
    // shutdown, so a quit taken mid-debounce still lands.
    void  SaveGlobalPrefsDeferred  ();
    void  FlushDeferredGlobalPrefs ();

    // A global-prefs write asked for but not yet made. See
    // SaveGlobalPrefsDeferred. The delay is long enough that a slider drag
    // writes once when it settles, short enough that it is over before the
    // user reaches for the window's close button.
    static constexpr UINT_PTR  kPrefsSaveTimerId = 0xCA55;
    static constexpr UINT      kPrefsSaveDelayMs = 750;

    // Persisted per-machine $cassoUiPrefs. Reads + merges the machine JSON,
    // handing back the "$cassoUiPrefs" object in outUiPrefs -- or null when it
    // is absent OR unreadable/corrupt, both recovered to defaults, never
    // fatal.
    void  LoadMachineUiPrefs (JsonValue & outDoc, const JsonValue * & outUiPrefs);

    // Writes one //c case-switch latch ("eightyColumnSwitch" / "keyboardDvorak")
    // into the current machine's $cassoUiPrefs so it survives across runs.
    void  PersistSwitchState (const char * key, bool value);

    // Records the active machine so the next launch boots it by default.
    void  RecordActiveMachineSelection ();

    // Writes the picked color mode into the machine's UI prefs, the same key
    // the Settings panel saves on OK.
    void  PersistColorModeForMachine (int settingsColorModeIndex);

    // Activates the named theme in ThemeManager (which notifies the chrome
    // cache listener) and persists the choice into GlobalUserPrefs. No-op if
    // the name is empty; falls back to Skeuomorphic if unknown.
    HRESULT  ApplyAndPersistTheme (const std::string & themeName);

    // Activates the named theme LIVE (reskins the chrome via the
    // ThemeManager listener) WITHOUT persisting it to GlobalUserPrefs.
    // Used by the Settings Theme page's "Apply now" affordance so the
    // user can preview a theme on the real chrome; a subsequent Cancel
    // re-activates the baseline theme, and OK persists via
    // ApplyAndPersistTheme. No-op if empty; falls back to Skeuomorphic.
    HRESULT  ApplyThemeLive (const std::string & themeName);

    // Settings > General: the two download offers, each saved immediately
    // like the other live toggles in Settings, and the settings folder.
    void  SetAudioDownloadConsent (const std::string & consent);
    void  SetRomRefreshConsent    (const std::string & consent);
    void  OpenSettingsFolder      ();

    // %LOCALAPPDATA%\Casso, where the preferences files live.
    static std::wstring  GetSettingsFolder ();

    // The Settings dialog, shown modeless so the emulator keeps running
    // behind it. `showControllers` lands on the Controllers tab.
    void              OpenSettings          (bool showControllers = false);
    SettingsSheet  *  GetSheet              () const { return m_settingsSheet.get(); }

    // Destroys a sheet that has closed, and says whether there was one.
    // Called from the message loop, a safe point -- not from inside the
    // sheet's own EndDialog handler.
    bool              TryDestroyClosedSheet ();

private:
    EmulatorShell                      & m_shell;

    // UI-thread filesystem: the settings panel and the config store resolve
    // paths through it on the UI thread.
    Win32FileSystem                      m_uiFs;

    // ThemeManager + UserConfigStore + GlobalUserPrefs are owned here and
    // handed to the SettingsSheet each time it opens (OpenSettings).
    std::unique_ptr<ThemeManager>        m_themeManager;
    std::unique_ptr<UserConfigStore>     m_userConfigStore;
    GlobalUserPrefs                      m_globalPrefs;

    // ATOMIC because two threads reach it: the UI thread arms it from the
    // toolbar callbacks, and the CPU thread clears it through the
    // SaveGlobalPrefs that SwitchMachine calls.
    std::atomic<bool>                    m_globalPrefsDirty = false;

    // Heap-owned + null when closed; OpenSettings creates it and the close
    // callback flags m_settingsSheetClosePending so the message loop destroys
    // it at a safe point (not from inside its own EndDialog handler).
    std::unique_ptr<SettingsSheet>       m_settingsSheet;
    bool                                 m_settingsSheetClosePending = false;
};
