#include "Pch.h"

#include "Shell/EmulatorShell.h"
#include "Shell/Components/ShellChrome.h"
#include "Shell/Components/ShellDeskScene.h"
#include "Ui/ThemeManager.h"
#include "Shell/Components/ShellSettings.h"
#include "Shell/Components/ShellDisks.h"
#include "Shell/Components/ShellTapeDeck.h"
#include "Shell/Components/ShellUpdater.h"
#include "Shell/EmulatorShellInternal.h"
#include "AssetBootstrap.h"
#include "Config/MonitorCatalog.h"
#include "Config/MachineInputPrefs.h"
#include "Controllers/ControllerProfileStore.h"
#include "Controllers/ControllerTokens.h"
#include "Controllers/PlayerModeRules.h"
#include "Core/JsonWriter.h"
#include "Config/CrtPresets.h"
#include "Config/CrtResolver.h"
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
#include "Machines/Apple2/Common/SiriusJoyport.h"
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
#include "Ui/Dialogs/DialogBodyContent.h"
#include "Ui/Dialogs/MessageDialog.h"
#include "Ui/Dialogs/SalvageDialogContent.h"
#include "Ui/Settings/SettingsPanelState.h"
#include "Ui/Settings/SettingsSheet.h"   // TEMP (T162 3a dev trigger)
#include "Seams/Win32IntentChannel.h"
#include "Devices/Disk/PreservedCopy.h"





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
        ColorUtil::ResolveColorMonitorTextArgb (m_settings->GetPrefs().colorMonitorTextMode,
                                                m_settings->GetPrefs().colorMonitorTextCustomArgb));
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadControllerPrefs
//
//  Hands the saved controller settings, calibrations, active profiles,
//  players and last holders to the service. Anything that cannot be used is
//  said once: it falls back to the default mapping, to automatic
//  calibration or to Automatic, and the next save drops it, so there is
//  nothing to say again.
//
//  THE PLAYERS ARE ADOPTED ONCE. With no players saved, the launched
//  machine's own selection becomes them and is saved at once, so the saved
//  players mark the adoption done and no other machine's selection is ever
//  read for it. Player 1's keys are taken here too, before anything can
//  reconcile Player 1 with a shell that has not heard of them yet.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::LoadControllerPrefs()
{
    ControllerProfileStore    store;
    std::vector<std::string>  rejected;
    PlayerEntries             entries;
    PlayerLastHolders         lastHolders;
    JsonValue                 doc;
    const JsonValue         * uiPrefs   = nullptr;
    bool                      isAdopted = false;



    if (m_controllerService == nullptr)
    {
        return;
    }

    store.FromJson (m_settings->GetPrefs().controllers, rejected);
    m_hadSavedPlayerModes = store.hasPlayerModes;

    m_controllerService->SetModelSettings  (store.models);
    m_controllerService->SetCalibrations   (store.calibrations);
    m_controllerService->SetActiveProfiles (ProfileMode::Joystick, store.activeProfiles);
    m_controllerService->SetActiveProfiles (ProfileMode::Paddle,   store.paddleActiveProfiles);
    m_controllerService->SetActiveProfiles (ProfileMode::Joyport,  store.joyportActiveProfiles);

    if (store.players.has_value())
    {
        entries     = store.players.value();
        lastHolders = store.lastHolders;
    }
    else
    {
        m_settings->LoadMachineUiPrefs (doc, uiPrefs);
        entries   = MachineInputPrefs::ReadAdoptedPlayers (uiPrefs, lastHolders);
        isAdopted = true;
    }

    m_controllerService->SetLastHolders   (lastHolders);
    m_controllerService->SetPlayerEntries (entries);

    m_arrowsJoystick = m_controllerService->GetPlayerEntries()[0].kind == PlayerEntryKind::ArrowKeys;

    if (isAdopted)
    {
        SaveControllerPrefs();
    }

    if (!rejected.empty())
    {
        m_chrome->PostNotice (L"Some saved controller settings couldn't be read, so those settings were reset.");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveControllerPrefs
//
//  Writes every controller model's settings, every unit's calibration, the
//  active profiles, the players' entries and the last holders into the
//  global prefs, and saves them only when that changed what they hold: an
//  automatic calibration that learned nothing new costs no write.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SaveControllerPrefs()
{
    ControllerProfileStore  store;
    JsonValue               controllers;



    if (m_controllerService == nullptr)
    {
        return;
    }

    store.models                = m_controllerService->GetModelSettings();
    store.calibrations          = m_controllerService->GetCalibrations();
    store.activeProfiles        = m_controllerService->GetActiveProfiles (ProfileMode::Joystick);
    store.paddleActiveProfiles  = m_controllerService->GetActiveProfiles (ProfileMode::Paddle);
    store.joyportActiveProfiles = m_controllerService->GetActiveProfiles (ProfileMode::Joyport);
    store.players               = m_controllerService->GetPlayerEntries();
    store.lastHolders           = m_controllerService->GetLastHolders();
    controllers                 = store.ToJson (m_settings->GetPrefs().controllers);

    if (JsonWriter::Write (controllers) == JsonWriter::Write (m_settings->GetPrefs().controllers))
    {
        return;
    }

    m_settings->GetPrefs().controllers = std::move (controllers);
    m_settings->SaveGlobalPrefs();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdoptInputModeForMachine
//
//  Seeds the live input mapping. Player 1's keys and mouse are the global
//  players' entries and follow the user from machine to machine; only the
//  //c's own mouse comes from the machine's $cassoUiPrefs block, and a
//  machine that has never stored it falls back to the legacy global setting,
//  so upgrading from a build where the mapping was global keeps it. The mouse
//  picked as Player 1's paddle takes the pointer, so it outranks that mouse.
//
//  STATE ONLY, no chrome. The machine-switch path calls this on the CPU
//  thread, and SyncSelectorState measures text through Dxui, which asserts
//  the UI thread; both callers reflect the state on the UI thread afterwards
//  (the switch through the post-switch reflow, launch through the layout that
//  follows). This is the same rule ApplyDefaultPointerForMachine follows, and
//  it runs before that one so the //c mouse nudge sees the restored value.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::AdoptInputModeForMachine (const JsonValue * uiPrefs, const std::string & machineId)
{
    PlayerEntryKind  playerOne     = PlayerEntryKind::Automatic;
    bool             machineArrows = false;



    MachineInputPrefs::ReadFromUiPrefs (uiPrefs,
                                        m_settings->GetPrefs().pointerMapping,
                                        machineArrows,
                                        m_pointerMode);

    if (m_controllerService != nullptr)
    {
        playerOne = m_controllerService->GetPlayerEntries()[0].kind;
    }

    m_arrowsJoystick = playerOne == PlayerEntryKind::ArrowKeys;

    if (playerOne == PlayerEntryKind::MousePaddle)
    {
        m_pointerMode = InputMappingMode::Paddle;
    }

    AdoptControllerForMachine (uiPrefs, machineId);

    // The mixer is thread-safe and writes on the UI thread, so handing the
    // axes over from here is safe on the CPU thread too.
    SyncGamePortAxisOwner();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdoptControllerForMachine
//
//  Sets the service up for the machine being entered: its axis count, the
//  move of its old active profile, a rate reset and a rescan. The players are
//  global and are not touched: they describe the controllers on the desk,
//  not the machine.
//
//  An UNREADABLE OR UNKNOWN TOKEN IS TREATED AS NO CONTROLLER, not as an
//  error: there is only no old profile to move.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::AdoptControllerForMachine (const JsonValue * uiPrefs, const std::string & machineId)
{
    HRESULT                              hr         = S_OK;
    std::string                          token;
    std::string                          legacyProfile;
    std::map<std::string, std::string>   normalProfiles;
    std::optional<ControllerUnitKey>     selection;
    ControllerUnitKey                    unit;
    bool                                 isAdopted  = false;
    const MachineDefinition            * definition = MachineDefinitions::Find (machineId);



    if (m_controllerService == nullptr)
    {
        return;
    }

    // How many axes this machine has, before anything is mapped onto them: a
    // player slot for paddles it lacks is kept and plays nothing (FR-035).
    //
    // From `machineId`, NOT from m_machine. On the switch path this runs
    // before the new config is adopted, so m_machine is still the machine
    // being LEFT: reading it left the count a machine behind, which gave a
    // //c player two the //e's PDL2/PDL3 -- no stick, but a live button --
    // and cost an //e player two the axes the machine does have.
    if (definition != nullptr)
    {
        m_controllerService->SetAxisCount (static_cast<size_t> (definition->gamePortAxisCount));
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

    // A machine's own active profile is from before each controller carried
    // its own. It passes to the machine's saved controller once, when that
    // controller has none recorded, and is written back to no machine after.
    legacyProfile  = MachineInputPrefs::ReadProfileName (uiPrefs);
    normalProfiles = m_controllerService->GetActiveProfiles (ProfileMode::Joystick);
    isAdopted      = ControllerProfileStore::TryAdoptLegacyProfile (normalProfiles, selection, legacyProfile);

    if (isAdopted)
    {
        m_controllerService->SetActiveProfiles (ProfileMode::Joystick, normalProfiles);
    }

    // A rate binding's paddle position belongs to the machine it was moved on.
    m_controllerService->ResetPaddleRate();

    // The players are decided against what is attached now.
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
//  Saves the live mapping. The players' entries, Player 1's keys and mouse
//  among them, go to the global prefs. The machine's $cassoUiPrefs block
//  takes only its pointer mapping, which is the //c's own mouse: a pointer
//  mode that holds the pointer is written as Off.
//
//  THE MACHINE'S OLD SELECTION IS LEFT ALONE. Its controller, two-player
//  block and arrows-to-joystick are neither written nor removed, so an older
//  build reading the same file keeps its own behavior.
//
//  Best-effort: a missing store or machine name, or a write failure, just
//  leaves the on-disk state as it was.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PersistInputModeForMachine()
{
    HRESULT                                         hr = S_OK;
    std::vector<std::pair<std::string, JsonValue>>  entries;



    SaveControllerPrefs();

    if (m_settings->GetConfigStore() == nullptr || m_machine.GetCurrentMachineName().empty())
    {
        return;
    }

    entries = MachineInputPrefs::BuildUiPrefEntries (m_pointerMode);

    hr = DiskSettings::WriteSavedUiPrefs (
             *m_settings->GetConfigStore(), m_settings->GetFileSystem(), m_machine.GetCurrentMachineName(), entries);

    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MigrateJoyportAtLaunch
//
//  The Joyport setting that per-player modes replaced is read only while no
//  player mode has been saved: a Joyport that was on puts Player 1 in the
//  left jack and Player 2 on Same as Player 1, and the players' modes are
//  saved at once, which marks it read (PlayerModeRules::MigrateAdapter). The
//  old global key is then removed from the global prefs; each machine's own
//  value is left for older builds. Then the machine just built takes the
//  players' modes.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::MigrateJoyportAtLaunch (const JsonValue * uiPrefs)
{
    JoyportMigration  migration = PlayerModeRules::MigrateAdapter (m_hadSavedPlayerModes, m_settings->GetPrefs().gamePortAdapter,
                                                                   uiPrefs, m_machine.GetJoyport() != nullptr);



    if (migration.isJoyport && m_controllerService != nullptr)
    {
        m_controllerService->SetPlayerEntries (PlayerModeRules::ApplyMigration (m_controllerService->GetPlayerEntries()));
    }

    // Saves only what changed, so a launch with the modes already saved
    // writes nothing here.
    SaveControllerPrefs();
    m_hadSavedPlayerModes = true;

    if (migration.shouldRemoveKey)
    {
        m_settings->GetPrefs().gamePortAdapter.clear();
        m_settings->SaveGlobalPrefs();
    }

    ApplyJoyportToMachine();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyJoyportToMachine
//
//  Attaches the running machine's Joyport while a player is in one of its
//  jacks, connects its paddle inputs while a player beside it stands in for
//  a paddle or a joystick in the rear sockets, and tells the controller
//  service whether the machine has one, so the players in its jacks play
//  their Joyport profiles. Runs once the machine is built -- at a cold start,
//  and on a machine switch right after the new devices are built, before the
//  power cycle that opens the Joyport's reset window -- and after every
//  change to the players' modes. The //c builds no Joyport, so there the
//  jacks play as joysticks and the modes are kept for the next machine that
//  has one.
//
//  Takes no lock: a machine switch calls it while holding the lifetime lock
//  exclusively, and SyncJoyport takes it shared.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ApplyJoyportToMachine()
{
    SiriusJoyport  * joyport    = m_machine.GetJoyport();
    bool             hasJoyport = joyport != nullptr;
    PlayerEntries    entries;



    if (m_controllerService)
    {
        entries = m_controllerService->GetPlayerEntries();
        m_controllerService->SetJoyportAvailable (hasJoyport);
    }

    if (joyport != nullptr)
    {
        joyport->SetPaddlesConnected (PlayerModeRules::ArePaddlesConnected (entries, hasJoyport));
        joyport->SetAttached         (PlayerModeRules::IsJoyportOn (entries, hasJoyport));
    }
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
    m_settings->GetThemeManager()->AddChangeListener ([this] (const LoadedTheme & t)
    {
        m_scene->ApplyChromeThemeByName (t.name);

        // The command bar's theme picker is built from this catalog, and the
        // manager outlives every other path that can change the active theme
        // (the picker itself, Settings, a fallback activation), so this is
        // the one place that sees all of them.
        m_chrome->RefreshToolbarThemeList();
    });

    // Tell the theme manager which machine is active BEFORE the
    // first Activate so its listener notification carries the
    // correctly-resolved (per-variant) theme.
    m_settings->GetThemeManager()->SetActiveMachineName (m_machine.GetConfig().name);

    //  The notices about a changed disk mention the machine, and "the Apple"
    //  is not what is in front of the user. Set beside the theme's copy so the
    //  two cannot come to disagree about which machine is running.
    m_machine.GetDiskStore().SetMachineName (m_machine.GetConfig().name);

    hrActivate = m_settings->GetThemeManager()->Activate (m_settings->GetPrefs().activeTheme);
    if (FAILED (hrActivate))
    {
        // The persisted theme name is unknown -- renamed, deleted, or a stale
        // default. Fall back to the canonical built-in. If even that is not in
        // the discovered set, chrome keeps its constructed Skeuomorphic
        // default, so a failed fallback is genuinely nothing to act on. This
        // function returns void, hence the explicit discard rather than CHR.
        hrActivate = m_settings->GetThemeManager()->Activate ("Skeuomorphic");
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



    m_settings->LoadMachineUiPrefs (doc, uiPrefs);

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

    // The players' modes are global and apply to a machine with no block of
    // its own too, so the Joyport is settled above the guard below.
    MigrateJoyportAtLaunch (uiPrefs);

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
            m_disks->SetExternalDriveConnected (!diskPort->device.empty());
        }
        else
        {
            hrOpt = uiPrefs->GetBool ("externalDriveConnected", extConnected);
            if (SUCCEEDED (hrOpt))
            {
                m_disks->SetExternalDriveConnected (extConnected);
            }
        }
    }

    hrOpt = uiPrefs->GetBool ("mouseConnected", mouseConn);
    if (SUCCEEDED (hrOpt))
    {
        m_mouseConnected = mouseConn;
    }

    // The cassette recorder, connected unless it was disconnected.
    m_tapeDeck->SetRecorderConnected (true);
    m_tapeDeck->LoadRecorderConnected (*uiPrefs);


    // Seed the per-drive user write-protect preference BEFORE the
    // command-line mount so the very first mount already re-asserts it
    // onto the image.
    hrOpt = uiPrefs->GetArray ("writeProtect", wpArr);
    if (SUCCEEDED (hrOpt) && wpArr != nullptr)
    {
        for (size_t wi = 0; wi < wpArr->GetArraySize() && wi < m_disks->GetUserWriteProtect().size(); ++wi)
        {
            const JsonValue &  entry = wpArr->GetArrayElement (wi);

            if (entry.GetType() == JsonType::Bool)
            {
                m_disks->GetUserWriteProtect()[wi] = entry.GetBool();
            }
        }
    }

Error:
    return;
}
