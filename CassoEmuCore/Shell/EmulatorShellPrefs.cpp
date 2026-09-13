#include "Pch.h"

#include "Shell/EmulatorShell.h"
#include "Shell/EmulatorShellInternal.h"
#include "AssetBootstrap.h"
#include "Config/MonitorCatalog.h"
#include "Config/MachineInputPrefs.h"
#include "Controllers/ControllerTokens.h"
#include "Config/CrtPresets.h"
#include "Config/CrtResolver.h"
#include "Ui/Chrome/DriveLabelTruncation.h"
#include "Print/PrintJobStore.h"
#include "Machines/Apple2/Common/PrinterCard.h"
#include "Ui/PrinterPanel.h"
#include "Core/PathResolver.h"
#include "Version.h"
#include "BuildInfo.h"
#include "resource.h"
#include "Devices/RamDevice.h"
#include "Devices/RomDevice.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Common/AppleSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleGamePort.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleSpeaker.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/MockingboardCard.h"
#include "Machines/Apple2/Common/LanguageCard.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"
#include "Machines/Apple2/Apple2c/Apple2cRomBank.h"
#include "Machines/MachineDefinitions.h"
#include "Shell/FramePacing.h"
#include "Shell/Input/AppleKeyMapping.h"
#include "Shell/Layout/DriveRowLayout.h"
#include "Machines/Apple2/Common/AppleMouse.h"
#include "Core/Prng.h"
#include "Config/DiskSettings.h"
#include "Core/UnicodeSymbols.h"
#include "Core/MachineConfig.h"
#include "Core/JsonParser.h"
#include "Machines/Apple2/Common/AppleTextMode.h"
#include "Machines/Apple2/Common/Apple80ColTextMode.h"
#include "Machines/Apple2/Common/AppleLoResMode.h"
#include "Machines/Apple2/Common/AppleHiResMode.h"
#include "Machines/Apple2/Common/AppleDoubleHiResMode.h"
#include "Video/PixelFormat.h"
#include "Video/MonochromeTint.h"
#include "Ui/Chrome/ChromeMetrics.h"
#include "Ui/DriveWidgetController.h"
#include "Shell/DiskMru.h"
#include "Window/DxuiHwndSource.h"
#include "Ui/Dialogs/DialogBodyContent.h"
#include "Ui/Dialogs/MessageDialog.h"
#include "Ui/Dialogs/SalvageDialogContent.h"
#include "Ui/Settings/SettingsPanelState.h"
#include "Ui/Settings/SettingsSheet.h"   // TEMP (T162 3a dev trigger)
#include "Seams/Win32IntentChannel.h"
#include "Devices/Disk/PreservedCopy.h"





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

void EmulatorShell::LoadMachineUiPrefs (
    JsonValue         & outDoc,
    const JsonValue * & outUiPrefs)
{
    HRESULT            hr                = S_OK;
    std::string        machineNameNarrow = GetCurrentMachineNameNarrow();
    JsonValue          defaultJson;
    JsonParseError     parseErr;
    std::ifstream      configFile;
    std::stringstream  ss;
    std::string        jsonText;
    std::wstring       configRelPath     = std::wstring (L"Machines\\") + m_machine.GetCurrentMachineName() +
                                           L"\\" + m_machine.GetCurrentMachineName() + L".json";
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
//  RestoreColorTextPref
//
//  The Color monitor's text tint is global -- it describes how the user wants
//  text to read, not anything about the machine -- so it is restored here, off
//  GlobalUserPrefs, rather than with the per-machine block. The input mapping
//  that used to be restored alongside it moved to ApplyPersistedChromePrefs
//  when it became per machine.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RestoreColorTextPref()
{
    SetColorMonitorTextArgbLive (
        ColorUtil::ResolveColorMonitorTextArgb (m_globalPrefs.colorMonitorTextMode,
                                                m_globalPrefs.colorMonitorTextCustomArgb));
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdoptInputModeForMachine
//
//  Seeds the live input mapping from a machine's $cassoUiPrefs block. A
//  machine that has never stored one falls back to the legacy global setting,
//  so upgrading from a build where the mapping was global keeps it.
//
//  STATE ONLY, no chrome. The machine-switch path calls this on the CPU
//  thread, and SyncSelectorState measures text through Dxui, which asserts
//  the UI thread; both callers reflect the state on the UI thread afterwards
//  (the switch through the post-switch reflow, launch through the layout that
//  follows). This is the same rule ApplyDefaultPointerForMachine follows, and
//  it runs before that one so the //c mouse nudge sees the restored value.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::AdoptInputModeForMachine (const JsonValue * uiPrefs)
{
    MachineInputPrefs::ReadFromUiPrefs (uiPrefs,
                                        m_globalPrefs.pointerMapping,
                                        m_arrowsJoystick,
                                        m_pointerMode);

    AdoptControllerForMachine (uiPrefs);

    // The mixer is thread-safe and writes on the UI thread, so handing the
    // axes over from here is safe on the CPU thread too.
    SyncGamePortAxisOwner();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdoptControllerForMachine
//
//  Hands the machine's remembered controller to the service, so the policy
//  starts from the user's choice rather than choosing for them.
//
//  An UNREADABLE OR UNKNOWN TOKEN IS TREATED AS NO CHOICE, not as an error.
//  The machine still runs, and the policy then picks up whatever is attached,
//  which is what a user with a broken prefs file wants to happen.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::AdoptControllerForMachine (const JsonValue * uiPrefs)
{
    HRESULT                           hr    = S_OK;
    std::string                       token;
    std::optional<ControllerUnitKey>  selection;
    ControllerUnitKey                 unit;



    if (m_controllerService == nullptr)
    {
        return;
    }

    token = MachineInputPrefs::ReadControllerToken (uiPrefs);

    if (!token.empty())
    {
        hr = ControllerTokens::UnitFromToken (token, unit);

        if (SUCCEEDED (hr))
        {
            selection = unit;
        }
    }

    m_controllerService->SetSelection (selection);

    // Whatever was saved, the policy decides against what is attached now: a
    // machine with nothing saved selects an attached controller, and one
    // whose saved controller is absent has it replaced (FR-011, FR-032).
    m_controllerService->RequestRescan();

    if (m_controllerThread != nullptr)
    {
        m_controllerThread->Wake();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PersistInputModeForMachine
//
//  Writes the live mapping into the current machine's $cassoUiPrefs block.
//  Both keys go in one call, so a change that moves both axes -- picking
//  Paddle drops arrows-to-joystick -- costs one read-modify-write.
//
//  Best-effort: a missing store or machine name, or a write failure, just
//  leaves the on-disk state as it was.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PersistInputModeForMachine()
{
    HRESULT                                         hr = S_OK;
    std::vector<std::pair<std::string, JsonValue>>  entries;
    std::vector<std::pair<std::string, JsonValue>>  controllerEntries;
    std::string                                     token;



    if (m_userConfigStore == nullptr || m_machine.GetCurrentMachineName().empty())
    {
        return;
    }

    entries = MachineInputPrefs::BuildUiPrefEntries (m_pointerMode);

    // The controller rides along in the same read-modify-write: choosing one
    // turns the arrows and the paddle off, so every change that touches one
    // of the three touches at least two of the keys.
    if (m_controllerService != nullptr)
    {
        // The saved controller, not the one in use: a takeover or a clear is
        // for the session only (FR-011).
        std::optional<ControllerUnitKey>  selection = m_controllerService->GetSnapshot().saved;

        if (selection.has_value())
        {
            token = ControllerTokens::UnitToToken (selection.value());
        }

        controllerEntries = MachineInputPrefs::BuildControllerEntries (token, std::string());
        entries.insert (entries.end(), controllerEntries.begin(), controllerEntries.end());
    }

    hr = DiskSettings::WriteSavedUiPrefs (
             *m_userConfigStore, m_uiFs, m_machine.GetCurrentMachineName(), entries);

    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RecordActiveMachineSelection
//
//  Records the currently-active machine so the next launch boots it by
//  default (Main resolves the value via this same GlobalUserPrefs field).
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RecordActiveMachineSelection()
{
    std::string  narrow = GetCurrentMachineNameNarrow();



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

void EmulatorShell::PersistColorModeForMachine (int settingsColorModeIndex)
{
    HRESULT                                         hr      = S_OK;
    std::vector<std::pair<std::string, JsonValue>>  entries;
    const char *                                    text    = nullptr;
    bool                                            inRange = settingsColorModeIndex >= 0 &&
                                                              settingsColorModeIndex <= (int) SettingsColorMode::White;



    if (m_userConfigStore == nullptr || m_machine.GetCurrentMachineName().empty() || !inRange)
    {
        return;
    }

    text = SettingsPanelState::ColorToString ((SettingsColorMode) settingsColorModeIndex);
    entries.emplace_back ("colorMode", JsonValue (std::string (text)));

    hr = DiskSettings::WriteSavedUiPrefs (*m_userConfigStore, m_uiFs,
                                          m_machine.GetCurrentMachineName(), entries);

    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SubscribeAndActivateTheme
//
//  Two orderings here, both load-bearing.
//
//  The change listener is subscribed BEFORE the first Activate, so that
//  initial activation fires it and primes m_chromeTheme from the persisted
//  user choice. Subscribing afterwards leaves the chrome painting its
//  constructed Skeuomorphic default until the user happens to re-pick their
//  theme in Settings -- a bug that looks like the preference was not saved.
//
//  The active machine name is set before Activate for the same reason: themes
//  resolve per machine variant, so the listener notification would otherwise
//  carry a theme resolved against the wrong machine.
//
//  A failed Activate means the persisted theme name no longer exists -- it was
//  renamed, deleted, or is a stale default -- so it falls back to the canonical
//  built-in. If even that is missing from the discovered set, the chrome keeps
//  its constructed default and there is genuinely nothing to act on, which is
//  why the fallback's result is explicitly discarded rather than propagated
//  (this function returns void by design; a missing theme is not a startup
//  failure).
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SubscribeAndActivateTheme()
{
    HRESULT  hrActivate = S_OK;



    // Subscribe the chrome theme cache to ThemeManager BEFORE we
    // activate, so the initial Activate() fires the listener and
    // primes m_chromeTheme from the persisted user choice. Without
    // this the chrome would still paint Skeuomorphic until the
    // user re-picked the theme in Settings.
    m_themeManager->AddChangeListener ([this] (const LoadedTheme & t)
    {
        m_chromeTheme = CassoTheme::MakeByName (t.name);
        ApplyThemeToChrome (m_chromeTheme);

        // The command bar's theme picker is built from this catalog, and the
        // manager outlives every other path that can change the active theme
        // (the picker itself, Settings, a fallback activation), so this is
        // the one place that sees all of them.
        RefreshToolbarThemeList();
    });

    // Tell the theme manager which machine is active BEFORE the
    // first Activate so its listener notification carries the
    // correctly-resolved (per-variant) theme.
    m_themeManager->SetActiveMachineName (m_machine.GetConfig().name);

    //  The notices about a changed disk mention the machine, and "the Apple"
    //  is not what is in front of the user. Set beside the theme's copy so the
    //  two cannot come to disagree about which machine is running.
    m_machine.GetDiskStore().SetMachineName (m_machine.GetConfig().name);

    hrActivate = m_themeManager->Activate (m_globalPrefs.activeTheme);
    if (FAILED (hrActivate))
    {
        // The persisted theme name is unknown -- renamed, deleted, or a stale
        // default. Fall back to the canonical built-in. If even that is not in
        // the discovered set, chrome keeps its constructed Skeuomorphic
        // default, so a failed fallback is genuinely nothing to act on. This
        // function returns void, hence the explicit discard rather than CHR.
        hrActivate = m_themeManager->Activate ("Skeuomorphic");
        IGNORE_RETURN_VALUE (hrActivate, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyPersistedChromePrefs
//
//  Applies the persisted per-machine colorMode + speed mode + //c peripheral
//  state at boot. Without this the emulator defaults to Color / Authentic
//  regardless of what the user last saved (MachineManager::SwitchMachine
//  carries the apply logic but only fires on user-initiated switches, not the
//  boot path).
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ApplyPersistedChromePrefs()
{
    HRESULT            hr           = S_OK;
    HRESULT            hrOpt        = S_OK;
    JsonValue          doc;
    const JsonValue *  uiPrefs      = nullptr;
    const JsonValue *  wpArr        = nullptr;
    std::string        speedMode;
    bool               extConnected = false;
    bool               mouseConn    = true;



    LoadMachineUiPrefs (doc, uiPrefs);

    // NO SAVED COLOR MEANS THE MONITOR'S OWN. The machine names the monitor
    // it ships with and the monitor owns its phosphor, so an untouched //c
    // comes up green because a Monitor //c is green -- not because anything
    // wrote "green" into a preference file for it.
    //
    // Applied ABOVE the guard below, because a machine carrying no
    // $cassoUiPrefs block at all is precisely the case where the monitor is
    // the only thing that can answer. Returning early for it left the shell
    // at the Color it constructs with, which is why a brand-new install came
    // up color exactly once: the window position written on the way out gave
    // the block something to hold, and the second launch reached this line
    // and found green.
    SetColorModeLive (MonitorCatalog::GetSettingsIndex (MonitorCatalog::GetColorModeForMachineJson (doc)));

    BAIL_OUT_IF (uiPrefs == nullptr, S_OK);

    // Each key below is optional: a fresh machine omits it, which the
    // getter reports as a failing HRESULT while leaving the target
    // untouched. We probe with hrOpt and apply only on success, so a
    // missing key keeps the built-in default -- a genuine corrupt-file
    // error already propagated out of LoadMachineUiPrefs above.

    // Speed mode (authentic / double / maximum) lives in the same UI prefs and,
    // like colorMode, must be pushed into CpuManager at boot. SwitchMachine
    // applies it for runtime machine switches, but the cold-boot path otherwise
    // leaves the CPU at its Authentic default while Settings shows the saved
    // value -- so a saved "maximum" never actually runs fast until re-picked.
    hrOpt = uiPrefs->GetString ("speedMode", speedMode);
    if (SUCCEEDED (hrOpt))
    {
        if      (speedMode == "authentic") { m_cpuManager.SetSpeedMode (SpeedMode::Authentic); }
        else if (speedMode == "double")    { m_cpuManager.SetSpeedMode (SpeedMode::Double);    }
        else if (speedMode == "maximum")   { m_cpuManager.SetSpeedMode (SpeedMode::Maximum);   }
    }

    // //c external drive + mouse: seed the connected states HERE -- before
    // FinishUiShellLayout gates on ShouldShowExternalDrive() -- so the first
    // paint matches the saved setup. External drive defaults not-connected;
    // mouse defaults CONNECTED.
    //
    // The drive's answer is the back-panel disk port. The legacy
    // externalDriveConnected boolean is still read as a FALLBACK because the
    // fold that retires it only runs on a version bump -- a config already at
    // the current stamp keeps its old key until Settings next saves, and
    // dropping the drive for that one launch would be a visible regression.
    {
        const PortConfig *  diskPort = m_machine.GetConfig().FindPort ("disk");

        if (diskPort != nullptr)
        {
            m_externalDriveConnected = !diskPort->device.empty();
        }
        else
        {
            hrOpt = uiPrefs->GetBool ("externalDriveConnected", extConnected);
            if (SUCCEEDED (hrOpt))
            {
                m_externalDriveConnected = extConnected;
            }
        }
    }

    hrOpt = uiPrefs->GetBool ("mouseConnected", mouseConn);
    if (SUCCEEDED (hrOpt))
    {
        m_mouseConnected = mouseConn;
    }

    // Seed the per-drive user write-protect preference BEFORE the
    // command-line mount so the very first mount already re-asserts it
    // onto the image.
    hrOpt = uiPrefs->GetArray ("writeProtect", wpArr);
    if (SUCCEEDED (hrOpt) && wpArr != nullptr)
    {
        for (size_t wi = 0; wi < wpArr->GetArraySize() && wi < m_userWriteProtect.size(); ++wi)
        {
            const JsonValue &  entry = wpArr->GetArrayElement (wi);

            if (entry.GetType() == JsonType::Bool)
            {
                m_userWriteProtect[wi] = entry.GetBool();
            }
        }
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ApplyAndPersistTheme
//
//  Activates the named theme via ThemeManager (which fires our chrome
//  cache listener) and writes the new choice into GlobalUserPrefs so
//  the next launch starts in the same theme. Activation failure on an
//  unknown name falls back to Skeuomorphic rather than leaving the
//  chrome in a stale state.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::ApplyAndPersistTheme (const std::string & themeName)
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
        hrSave = m_globalPrefs.Save (m_machine.GetAssetBaseDir(), m_uiFs);
    }

    IGNORE_RETURN_VALUE (hrSave, S_OK);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ApplyThemeLive
//
//  Activates the named theme via ThemeManager (which fires our chrome
//  cache listener and reskins the live chrome) but does NOT write the
//  choice into GlobalUserPrefs -- so a Settings Cancel can revert to the
//  baseline theme without a persisted trace. Mirrors ApplyAndPersistTheme
//  minus the save. Unknown names fall back to Skeuomorphic.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::ApplyThemeLive (const std::string & themeName)
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
//  EmulatorShell::SaveGlobalPrefs
//
//  Flushes the in-memory GlobalUserPrefs to UserPrefs.json. Used as the
//  WindowManager save callback so per-monitor window placement edits
//  land on disk immediately after the user moves/resizes the window.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SaveGlobalPrefs()
{
    HRESULT  hr          = S_OK;
    bool     offUiThread = (m_hwnd != nullptr) &&
                           (GetWindowThreadProcessId (m_hwnd, nullptr) != GetCurrentThreadId());



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
//  EmulatorShell::SaveGlobalPrefsDeferred
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

void EmulatorShell::SaveGlobalPrefsDeferred()
{
    HRESULT  hr = S_OK;



    m_globalPrefsDirty = true;

    // No window to hang a timer on -- before Initialize built one, or after
    // teardown destroyed it. The flag stands, and the shutdown flush writes
    // it. Tested against the HWND rather than the host because the Dxui timer
    // calls assert on a host without one.
    if (m_hwnd == nullptr || m_host == nullptr)
    {
        return;
    }

    hr = m_host->SetTimer (kPrefsSaveTimerId, kPrefsSaveDelayMs);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::FlushDeferredGlobalPrefs
//
//  Writes a pending deferred save now, if there is one. Called from the timer
//  and again at shutdown, so a quit taken inside the debounce window still
//  lands the user's last change.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::FlushDeferredGlobalPrefs()
{
    if (m_globalPrefsDirty)
    {
        SaveGlobalPrefs();
    }
}
