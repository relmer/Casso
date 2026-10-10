#include "Pch.h"

#include "Shell/Components/ShellSettings.h"
#include "Config/DiskSettings.h"
#include "Config/UserConfigStore.h"
#include "Core/JsonParser.h"
#include "Core/PathResolver.h"
#include "Shell/Components/ShellUpdater.h"
#include "Shell/EmulatorShell.h"
#include "Ui/Settings/SettingsPanelState.h"
#include "Ui/Settings/SettingsSheet.h"
#include "Ui/ThemeManager.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShellSettings
//
////////////////////////////////////////////////////////////////////////////////

ShellSettings::ShellSettings (EmulatorShell & shell)
    : m_shell (shell)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~ShellSettings
//
//  Out of line so the store, the theme catalog and the sheet are complete
//  where their unique_ptrs destroy them.
//
////////////////////////////////////////////////////////////////////////////////

ShellSettings::~ShellSettings() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellSettings::CreateConfigStore
//
////////////////////////////////////////////////////////////////////////////////

void ShellSettings::CreateConfigStore (const std::wstring & assetBaseDir)
{
    m_userConfigStore = std::make_unique<UserConfigStore> (assetBaseDir);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellSettings::CreateThemeManager
//
////////////////////////////////////////////////////////////////////////////////

void ShellSettings::CreateThemeManager (const std::wstring & themesDir)
{
    m_themeManager = std::make_unique<ThemeManager> (m_uiFs, themesDir);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellSettings::TryDestroyClosedSheet
//
////////////////////////////////////////////////////////////////////////////////

bool ShellSettings::TryDestroyClosedSheet()
{
    bool  isClosed = m_settingsSheetClosePending;



    if (isClosed)
    {
        m_settingsSheet.reset();
        m_settingsSheetClosePending = false;
    }

    return isClosed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadMachineUiPrefs
//
//  Reads the active machine's JSON config and merges the user overrides via
//  UserConfigStore, handing back the "$cassoUiPrefs" object in outUiPrefs.
//  Any problem collapses to outUiPrefs == nullptr so the caller keeps the
//  built-in defaults, and these cosmetic per-machine prefs never block
//  startup -- though corrupt content (as opposed to a simply-absent file or
//  key) asserts first so a debug build catches it. The returned pointer
//  aliases into outDoc, so outDoc must outlive every use of it.
//
////////////////////////////////////////////////////////////////////////////////

void ShellSettings::LoadMachineUiPrefs (
    JsonValue         & outDoc,
    const JsonValue * & outUiPrefs)
{
    HRESULT            hr                = S_OK;
    std::string        machineNameNarrow = m_shell.GetCurrentMachineNameNarrow();
    JsonValue          defaultJson;
    JsonParseError     parseErr;
    std::ifstream      configFile;
    std::stringstream  ss;
    std::string        jsonText;
    std::wstring       configRelPath     = std::wstring (L"Machines\\") + m_shell.m_machine.GetCurrentMachineName() +
                                           L"\\" + m_shell.m_machine.GetCurrentMachineName() + L".json";
    fs::path           configPath        = PathResolver::FindFile (PathResolver::BuildSearchPaths (
                                               PathResolver::GetExecutableDirectory(),
                                               PathResolver::GetWorkingDirectory()),
                                               configRelPath);



    outUiPrefs = nullptr;

    // A missing file, or a missing "$cassoUiPrefs" key, is normal (first run
    // for this machine): recover to null so the caller keeps defaults, no
    // assert. A machine's own config failing to parse IS a coding error -- it
    // is a shipped asset, not something a user edits -- so that one asserts.
    //
    // The store's Load is a different matter and must NOT assert. It reads the
    // user's prefs file, and PrimeChromeThemeEarly's banner already settles
    // what that means: a malformed prefs file is bad DATA, there is no bug for
    // a developer to break into, and it would stop the debugger every time
    // someone hand-edits their JSON. An unreadable file that could not be set
    // aside reaches here on the very next machine load.
    BAIL_OUT_IF (configPath.empty(), S_OK);
    configFile.open (configPath);
    BAIL_OUT_IF (!configFile.good(), S_OK);

    ss << configFile.rdbuf();
    jsonText = ss.str();

    hr = JsonParser::Parse (jsonText, defaultJson, parseErr);
    CHRA (hr);

    hr = m_userConfigStore->Load (machineNameNarrow, defaultJson, m_uiFs, outDoc);
    CHR (hr);

    BAIL_OUT_IF (outDoc.GetType() != JsonType::Object, S_OK);

    hr = outDoc.GetObject ("$cassoUiPrefs", outUiPrefs);
    if (FAILED (hr))
    {
        outUiPrefs = nullptr;
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RecordActiveMachineSelection
//
//  Records the currently-active machine so the next launch boots it by
//  default (Main resolves the value via this same GlobalUserPrefs field).
//
////////////////////////////////////////////////////////////////////////////////

void ShellSettings::RecordActiveMachineSelection()
{
    std::string  narrow = m_shell.GetCurrentMachineNameNarrow();



    if (m_globalPrefs.lastSelectedMachine != narrow)
    {
        m_globalPrefs.lastSelectedMachine = narrow;
        SaveGlobalPrefs();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PersistColorModeForMachine
//
//  Writes the picked color mode into the machine's UI prefs, the same key
//  the Settings panel saves on OK. The View menu's color commands
//  deliberately do not persist -- they are a momentary look -- but a picker
//  that shows the current value has to remember the one it was given.
//
////////////////////////////////////////////////////////////////////////////////

void ShellSettings::PersistColorModeForMachine (int settingsColorModeIndex)
{
    HRESULT                                         hr      = S_OK;
    std::vector<std::pair<std::string, JsonValue>>  entries;
    const char *                                    text    = nullptr;
    bool                                            inRange = settingsColorModeIndex >= 0 &&
                                                              settingsColorModeIndex <= (int) SettingsColorMode::White;



    if (m_userConfigStore == nullptr || m_shell.m_machine.GetCurrentMachineName().empty() || !inRange)
    {
        return;
    }

    text = SettingsPanelState::ColorToString ((SettingsColorMode) settingsColorModeIndex);
    entries.emplace_back ("colorMode", JsonValue (std::string (text)));

    hr = DiskSettings::WriteSavedUiPrefs (*m_userConfigStore, m_uiFs,
                                          m_shell.m_machine.GetCurrentMachineName(), entries);

    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellSettings::ApplyAndPersistTheme
//
//  Activates the named theme via ThemeManager (which fires our chrome
//  cache listener) and writes the new choice into GlobalUserPrefs so
//  the next launch starts in the same theme. Activation failure on an
//  unknown name falls back to Skeuomorphic rather than leaving the
//  chrome in a stale state.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ShellSettings::ApplyAndPersistTheme (const std::string & themeName)
{
    HRESULT      hr         = S_OK;
    HRESULT      hrActivate = S_OK;
    HRESULT      hrSave     = S_OK;
    std::string  resolved   = themeName;



    CBRA (m_themeManager);                       // null member = Casso bug
    BAIL_OUT_IF (themeName.empty(), S_OK);        // no theme requested -> no-op

    hrActivate = m_themeManager->Activate (themeName);
    if (FAILED (hrActivate))
    {
        resolved   = "Skeuomorphic";
        hrActivate = m_themeManager->Activate (resolved);
    }

    // Live guard now. Previously Activate reported "no such theme" as
    // S_FALSE, so CHR treated it as success and this function went on to
    // persist a theme name that never activated.
    CHR (hrActivate);

    m_globalPrefs.activeTheme = resolved;
    if (m_userConfigStore != nullptr)
    {
        hrSave = m_userConfigStore->SaveAll (m_globalPrefs, m_uiFs);
    }
    else
    {
        hrSave = m_globalPrefs.Save (m_shell.m_machine.GetAssetBaseDir(), m_uiFs);
    }

    IGNORE_RETURN_VALUE (hrSave, S_OK);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellSettings::ApplyThemeLive
//
//  Activates the named theme via ThemeManager (which fires our chrome
//  cache listener and reskins the live chrome) but does NOT write the
//  choice into GlobalUserPrefs -- so a Settings Cancel can revert to the
//  baseline theme without a persisted trace. Mirrors ApplyAndPersistTheme
//  minus the save. Unknown names fall back to Skeuomorphic.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ShellSettings::ApplyThemeLive (const std::string & themeName)
{
    HRESULT  hr         = S_OK;
    HRESULT  hrActivate = S_OK;



    CBRA (m_themeManager);                       // null member = Casso bug
    BAIL_OUT_IF (themeName.empty(), S_OK);        // no theme requested -> no-op

    hrActivate = m_themeManager->Activate (themeName);
    if (FAILED (hrActivate))
    {
        hrActivate = m_themeManager->Activate ("Skeuomorphic");
    }

    CHR (hrActivate);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellSettings::SaveGlobalPrefs
//
//  Flushes the in-memory GlobalUserPrefs to UserPrefs.json. Used as the
//  WindowManager save callback so per-monitor window placement edits
//  land on disk immediately after the user moves/resizes the window.
//
////////////////////////////////////////////////////////////////////////////////

void ShellSettings::SaveGlobalPrefs()
{
    HRESULT  hr          = S_OK;
    bool     offUiThread = (m_shell.m_hwnd != nullptr) &&
                           (GetWindowThreadProcessId (m_shell.m_hwnd, nullptr) != GetCurrentThreadId());



    if (m_userConfigStore == nullptr)
    {
        return;
    }

    hr = m_userConfigStore->SaveAll (m_globalPrefs, m_uiFs);

    // A deferred request is consumed only by a write that LANDED and that ran
    // on the thread the request was made from. Clearing it up front dropped the
    // change outright: a save that failed, or one skipped for want of a store,
    // still ate the request, and the shutdown flush writes nothing when the flag
    // is clear. Clearing it from the CPU thread -- SwitchMachine reaches here --
    // ate a request for a value that thread has no happens-before edge to, so
    // the file could be written with the old volume while the pending write that
    // would have corrected it was cancelled.
    //
    // The timer is deliberately NOT killed here: the Dxui timer calls assert the
    // UI thread. It fires once more and either finds nothing dirty and stops
    // itself in OnTimer, or writes the value a failed or off-thread save missed.
    if (SUCCEEDED (hr) && !offUiThread)
    {
        m_globalPrefsDirty = false;
    }

    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellSettings::SaveGlobalPrefsDeferred
//
//  Records that GlobalUserPrefs needs writing and (re)arms the timer that
//  writes it, so a burst of changes costs one file write instead of one per
//  change.
//
//  RE-ARMING ON EACH CALL is what makes it a debounce rather than a period:
//  the write happens once the changes stop, not on a fixed cadence through
//  the middle of a drag.
//
//  Before there is a window there is no timer to arm, so the request stands
//  as a dirty flag until something flushes it -- an ordinary SaveGlobalPrefs
//  from another setting, or shutdown.
//
////////////////////////////////////////////////////////////////////////////////

void ShellSettings::SaveGlobalPrefsDeferred()
{
    HRESULT  hr = S_OK;



    m_globalPrefsDirty = true;

    // No window to hang a timer on -- before Initialize built one, or after
    // teardown destroyed it. The flag stands, and the shutdown flush writes
    // it. Tested against the HWND rather than the host because the Dxui timer
    // calls assert on a host without one.
    if (m_shell.m_hwnd == nullptr || m_shell.m_host == nullptr)
    {
        return;
    }

    hr = m_shell.m_host->SetTimer (kPrefsSaveTimerId, kPrefsSaveDelayMs);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellSettings::FlushDeferredGlobalPrefs
//
//  Writes a pending deferred save now, if there is one. Called from the timer
//  and again at shutdown, so a quit taken inside the debounce window still
//  lands the user's last change.
//
////////////////////////////////////////////////////////////////////////////////

void ShellSettings::FlushDeferredGlobalPrefs()
{
    if (m_globalPrefsDirty)
    {
        SaveGlobalPrefs();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellSettings::SetAudioDownloadConsent
//
////////////////////////////////////////////////////////////////////////////////

void ShellSettings::SetAudioDownloadConsent (const std::string & consent)
{
    m_globalPrefs.audioDownloadConsent = consent;
    SaveGlobalPrefs();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellSettings::SetRomRefreshConsent
//
////////////////////////////////////////////////////////////////////////////////

void ShellSettings::SetRomRefreshConsent (const std::string & consent)
{
    m_globalPrefs.romRefreshConsent = consent;
    SaveGlobalPrefs();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellSettings::GetSettingsFolder
//
//  %LOCALAPPDATA%\Casso, where the preferences files live. Empty when the
//  folder cannot be resolved.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ShellSettings::GetSettingsFolder()
{
    return PathResolver::GetLocalAppDataDir (L"Casso").wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellSettings::OpenSettingsFolder
//
//  Opens the settings folder in Explorer.
//
////////////////////////////////////////////////////////////////////////////////

void ShellSettings::OpenSettingsFolder()
{
    std::wstring  folder = GetSettingsFolder();



    if (!folder.empty())
    {
        ShellUpdater::OpenUrl (folder);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenSettings
//
//  Opens the Settings dialog (View > Settings / Ctrl+,). The bespoke
//  SettingsPanel + SettingsWindow were retired in T162 slice 3d; this shows
//  the DxuiPropertySheet-based SettingsSheet MODELESS (FR-041) so the emulator
//  keeps running behind it. The sheet is heap-owned; its close callback flags
//  a deferred destroy handled by RunMessageLoop. A second invocation while it
//  is already open just re-focuses the existing sheet.
//
//  showControllers lands on the Controllers tab, for Controller settings...,
//  whether the sheet is opening or already open.
//
////////////////////////////////////////////////////////////////////////////////

void ShellSettings::OpenSettings (bool showControllers)
{
    HINSTANCE  hInst = (HINSTANCE) GetWindowLongPtrW (m_shell.m_hwnd, GWLP_HINSTANCE);



    if (m_settingsSheet != nullptr)
    {
        HWND  existing = m_settingsSheet->GetHwnd();
        if (existing != nullptr)
        {
            SetForegroundWindow (existing);
        }

        if (showControllers)
        {
            m_settingsSheet->ShowControllersPage();
        }

        return;
    }

    m_settingsSheet = std::make_unique<SettingsSheet>();
    // A size the user dragged it to is kept for the next time it opens.
    m_settingsSheet->SetOnDialogEnd ([this] (int)
    {
        if (m_settingsSheet->TryStoreResizedSize())
        {
            SaveGlobalPrefs();
        }

        m_settingsSheetClosePending = true;
    });

    (void) m_settingsSheet->OpenModeless (hInst, m_shell.m_hwnd,
                                          *m_userConfigStore, m_globalPrefs, *m_themeManager,
                                          m_shell, m_uiFs);

    if (showControllers)
    {
        m_settingsSheet->ShowControllersPage();
    }
}
