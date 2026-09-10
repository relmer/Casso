#include "Pch.h"

#include "MachineManager.h"

#include "../EmulatorShell.h"
#include "../AssetBootstrap.h"
#include "Config/DiskSettings.h"
#include "../resource.h"
#include "Config/MonitorCatalog.h"
#include "Core/PathResolver.h"
#include "Core/MachineConfig.h"
#include "Core/CpuFactory.h"
#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"
#include "Core/Prng.h"
#include "Devices/RamDevice.h"
#include "Devices/RomDevice.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Common/AppleSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleGamePort.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleSpeaker.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Devices/Acia6551.h"
#include "Devices/AciaEndpoints.h"
#include "Machines/Apple2/Common/MockingboardCard.h"
#include "Machines/Apple2/Common/LanguageCard.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"
#include "Machines/Apple2/Apple2c/Apple2cRomBank.h"
#include "Machines/Apple2/Common/AppleMouse.h"
#include "Machines/Apple2/Common/ParallelFirmware.h"
#include "Machines/Apple2/Common/PrinterCard.h"
#include "Devices/Printer/PrintRaster.h"
#include "Print/PrintJobStore.h"
#include "Machines/Apple2/Common/AppleTextMode.h"
#include "Machines/Apple2/Common/Apple80ColTextMode.h"
#include "Machines/Apple2/Common/AppleLoResMode.h"
#include "Machines/Apple2/Common/AppleHiResMode.h"
#include "Machines/Apple2/Common/AppleDoubleHiResMode.h"
#include "Audio/DriveAudioMixer.h"
#include "Machines/Apple2/Common/Disk2AudioSource.h"
#include "Shell/CpuManager.h"
#include "Shell/DiskManager.h"
#include "../Ui/Disk2DebugPanel.h"
#include "../Ui/InputDebugPanel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ResolveMachineSpeedCommand
//
//  Digs the saved speed mode out of the merged config and maps it to the
//  IDM_* the command router expects. 0 means "no saved preference", which
//  every step below can produce: no object, no $cassoUiPrefs, no
//  speedMode key, or a value this build does not recognize.
//
////////////////////////////////////////////////////////////////////////////////

WORD  MachineManager::ResolveMachineSpeedCommand (const JsonValue & mergedJson)
{
    HRESULT            hr      = S_OK;
    const JsonValue *  uiPrefs = nullptr;
    std::string        speed;
    WORD               command = 0;



    if (mergedJson.GetType() == JsonType::Object)
    {
        hr = mergedJson.GetObject ("$cassoUiPrefs", uiPrefs);
    }

    if (SUCCEEDED (hr) && uiPrefs != nullptr)
    {
        _Analysis_assume_ (uiPrefs != nullptr);

        hr = uiPrefs->GetString ("speedMode", speed);
    }

    if (SUCCEEDED (hr))
    {
        if      (speed == "authentic") { command = IDM_MACHINE_SPEED_1X;  }
        else if (speed == "double")    { command = IDM_MACHINE_SPEED_2X;  }
        else if (speed == "maximum")   { command = IDM_MACHINE_SPEED_MAX; }
    }

    return command;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineManager
//
////////////////////////////////////////////////////////////////////////////////

MachineManager::MachineManager (EmulatorShell & shell)
    : m_shell (shell)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShowMachinePicker
//
//  Legacy `MachinePickerDialog` is retired (FR-027). The consolidated
//  Settings panel hosts the machine selector and routes the actual
//  switch through `SwitchMachine` / `SettingsPanelState::Apply` on
//  commit. Old entry points (`IDM_FILE_OPEN`, status-bar machine cell)
//  funnel here so they keep working with no behavioral surprise to
//  the user.
//
////////////////////////////////////////////////////////////////////////////////

void MachineManager::ShowMachinePicker()
{
    OutputDebugStringA ("[MachineManager] ShowMachinePicker unavailable in native-only baseline.\n");
}





////////////////////////////////////////////////////////////////////////////////
//
//  SwitchMachine
//
//  Replaces the running machine in place: load the new config, tear the old
//  one down, rebuild, and carry forward what the user would expect to survive.
//
//  Runs on the CPU THREAD, which shapes several decisions below. Anything
//  UI-facing is posted rather than called -- the color-mode application goes
//  through PostMessage instead of the command dispatcher, because that
//  dispatcher is a UI-thread surface and today's harmless atomic store would
//  quietly inherit this thread the moment anything else were added to it.
//
//  Assets are NOT fetched here. ROM and disk-audio bootstrap happens on the UI
//  thread in ShowMachinePicker before the switch is enqueued, so by the time
//  this runs everything the new machine needs is already on disk.
//
//  The machine is built from the USER-MERGED config, not the shipped config
//  text. Without that, a machine-level edit -- a slot disabled in Settings >
//  Hardware, say -- would apply only to the live speed and color and silently
//  revert on every switch or reboot. The extra $cassoUiPrefs and version keys
//  the merge carries are ignored by the loader.
//
//  Teardown order is the delicate half, and every step is placed against a
//  reference that would otherwise dangle:
//
//    debug panels    hold raw pointers into the OLD CPU's cycle counter
//    event sinks     keyboard, //e soft switches, and game port all point at
//                    panels that outlive the devices
//    printer worker  its drain thread holds a reference into the card's ring,
//                    which clearing the owned devices is about to free
//    audio mixer     borrows PSG sources owned by the Mockingboard card
//    IRQ tokens      reclaimed before their holders are destroyed
//    //c ROM bank    references the language card and the MMU
//
//  The refs are reset AS A WHOLE rather than field by field. It is a struct of
//  observer pointers into the owning collections, so resetting it wholesale
//  keeps the "every observer dies with its owner" invariant from rotting as
//  observers are added. The MMU needs its own explicit reset because it survives
//  across switches and is only reassigned when the new config carries an
//  apple2e-mmu -- otherwise a //e to ][ switch keeps a stale RamDevice pointer.
//
//  Three things deliberately CARRY ACROSS the switch:
//
//    dirty disks     flushed before teardown, so user writes are not lost
//    mounted disks   re-mounted on the new machine. The mental model is
//                    physical -- the user changed the computer, not the disk
//                    in the drive -- and re-mounting also updates the new
//                    machine's prefs so it sticks on later launches
//    pending print   persisted while the host still holds the outgoing
//                    OUTGOING machine, so the strip lands in its folder
//                    (FR-026)
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineManager::SwitchMachine (const std::wstring & machineName)
{
    HRESULT                hr                = S_OK;
    std::vector<fs::path>  searchPaths;
    fs::path               configRelPath;
    fs::path               configPath;
    std::ifstream          configFile;
    bool                   configGood        = false;
    std::stringstream      ss;
    std::string            jsonText;
    std::vector<fs::path>  romSearchPaths;
    std::string            error;
    MachineConfig          newConfig;
    std::string            machineNameNarrow = fs::path (machineName).string();
    JsonValue              defaultJson;
    JsonValue              mergedJson;
    JsonParseError         parseErr;
    WORD                   speedCmd          = 0;
    bool                   foundConfig       = false;
    HRESULT                hrParse           = S_OK;
    HRESULT                hrMerge           = S_OK;
    std::string            carryDisk1;
    std::string            carryDisk2;
    const JsonValue *      inputUiPrefs      = nullptr;



    // Find and load the new machine config. ROM/disk-audio asset
    // bootstrap happens on the UI thread in ShowMachinePicker before
    // the switch command is enqueued; by the time we're here, every
    // asset the new machine needs is already on disk.
    searchPaths   = PathResolver::BuildSearchPaths (PathResolver::GetExecutableDirectory(),
                                                     PathResolver::GetWorkingDirectory());
    configRelPath = fs::path ("Machines") / machineNameNarrow
                                          / (machineNameNarrow + ".json");
    configPath    = PathResolver::FindFile (searchPaths, configRelPath);

    foundConfig = !configPath.empty();
    CBRN (foundConfig,
          std::format (L"Machine config not found: {}", machineName).c_str());

    configFile.open (configPath);
    configGood = configFile.good();
    CBRN (configGood,
          std::format (L"Cannot open machine config:\n{}", configPath.wstring()).c_str());

    ss << configFile.rdbuf();
    jsonText = ss.str();

    hrParse = JsonParser::Parse (jsonText, defaultJson, parseErr);

    if (SUCCEEDED (hrParse) && m_shell.m_userConfigStore != nullptr)
    {
        hrMerge = m_shell.m_userConfigStore->Load (machineNameNarrow,
                                                   defaultJson,
                                                   m_shell.m_uiFs,
                                                   mergedJson);

        if (SUCCEEDED (hrMerge))
        {
            speedCmd = ResolveMachineSpeedCommand (mergedJson);

            // Push the persisted colorMode into the running shell so
            // the screen actually reflects what the user saved last
            // session. Without this the live colorMode stays at its
            // default until the user opens Settings -- which then
            // makes Cancel snap the screen to the persisted value
            // because the panel reads the baseline from prefs, not
            // from the live shell state.
            if (mergedJson.GetType() == JsonType::Object)
            {
                const JsonValue *  uiPrefs   = nullptr;
                std::string        colorMode;

                // THE MONITOR'S OWN PHOSPHOR, not zero and not a fixed
                // default: a machine with no saved color mode must still be
                // told what to show. Leaving it unset applied nothing at all,
                // and what the screen kept was the mode of the machine being
                // switched AWAY from -- which is how the //c came up green
                // after an //e and white after an Enhanced //e, with nothing
                // about the //c deciding either.
                WORD               colorCmd  = MonitorCatalog::PhosphorCommand (
                                                   MonitorCatalog::ForMachineJson (mergedJson));

                if (mergedJson.HasObject ("$cassoUiPrefs", uiPrefs) &&
                    uiPrefs != nullptr &&
                    uiPrefs->HasString ("colorMode", colorMode))
                {
                    if      (colorMode == "green")  { colorCmd = IDM_VIEW_GREEN; }
                    else if (colorMode == "amber")  { colorCmd = IDM_VIEW_AMBER; }
                    else if (colorMode == "white")  { colorCmd = IDM_VIEW_WHITE; }
                }

                // SwitchMachine runs on the CPU thread: route through the
                // message loop, not HandleCommand directly -- the command
                // dispatcher is a UI-thread surface (today the color handler
                // is an atomic store, but anything added to it would inherit
                // this thread; see the ApplyDefaultPointerForMachine assert).
                PostMessageW (m_shell.m_hwnd, WM_COMMAND, MAKEWPARAM (colorCmd, 0), 0);

                // //c external drive: adopt the switched-to machine's persisted
                // connected state so the second drive-mount widget matches the
                // saved setting once ReflowChromeForMachineChange relays the
                // chrome. Defaults to not-connected; harmless on non-//c
                // machines (ShouldShowExternalDrive ignores it when the system
                // ROM is not banked).
                {
                    const JsonValue  * extPrefs   = nullptr;
                    const JsonValue  * portsArray = nullptr;
                    bool               connected  = false;
                    bool               mouseConn  = false;
                    bool               fFromPort  = false;

                    if (mergedJson.HasObject ("$cassoUiPrefs", extPrefs) &&
                        extPrefs != nullptr)
                    {
                        HRESULT  hrExt = extPrefs->GetBool ("externalDriveConnected", connected);
                        IGNORE_RETURN_VALUE (hrExt, S_OK);
                    }

                    // The back-panel disk port is the answer when the machine
                    // declares one; the legacy boolean above stays as the
                    // fallback for a config that has not been folded yet.
                    if (mergedJson.HasArray ("ports", portsArray) &&
                        portsArray != nullptr)
                    {
                        for (size_t p = 0; !fFromPort && p < portsArray->GetArraySize(); p++)
                        {
                            const JsonValue  & port       = portsArray->GetArrayElement (p);
                            string             portName;
                            string             portDevice;
                            HRESULT            hrName     = S_OK;
                            HRESULT            hrDev      = S_OK;

                            if (port.GetType() != JsonType::Object)
                            {
                                continue;
                            }

                            hrName = port.GetString ("name",   portName);
                            hrDev  = port.GetString ("device", portDevice);

                            IGNORE_RETURN_VALUE (hrName, S_OK);
                            IGNORE_RETURN_VALUE (hrDev,  S_OK);

                            if (portName == "disk")
                            {
                                connected = !portDevice.empty();
                                fFromPort = true;
                            }
                        }
                    }

                    m_shell.m_externalDriveConnected = connected;

                    // //c mouse peripheral: adopt the switched-to machine's
                    // persisted connected state (default CONNECTED).
                    mouseConn = true;
                    if (extPrefs != nullptr)
                    {
                        HRESULT  hrM = extPrefs->GetBool ("mouseConnected", mouseConn);
                        IGNORE_RETURN_VALUE (hrM, S_OK);
                    }

                    m_shell.m_mouseConnected = mouseConn;

                    // The block the switched-to machine's input mapping is
                    // restored from. HELD, not applied: the config loader
                    // below can still refuse the switch, and the mapping
                    // belongs to whichever machine ends up running. It points
                    // into mergedJson, which outlives the adopt.
                    inputUiPrefs = extPrefs;
                }
            }
        }
    }

    romSearchPaths.push_back (configPath.parent_path().parent_path().parent_path());

    for (const auto & p : searchPaths)
    {
        if (p != romSearchPaths[0])
        {
            romSearchPaths.push_back (p);
        }
    }

    // Build the machine from the user-delta-merged config, not the base config
    // text, so machine-level edits -- e.g. a slot the user disabled in
    // Settings > Hardware (slots[].enabled=false) -- actually take effect on a
    // switch/reboot instead of only the live-applied speed/color. Falls back to
    // the base text when there is no merged result (no user delta). The extra
    // $cassoUiPrefs / version keys the merge carries are ignored by the loader.
    if (mergedJson.GetType() == JsonType::Object)
    {
        jsonText = JsonWriter::Write (mergedJson);
    }

    hr = MachineConfigLoader::Load (jsonText,
                                    machineNameNarrow,
                                    romSearchPaths,
                                    newConfig,
                                    error);
    CHRN (hr, std::format (L"Failed to load machine config:\n{}",
                           std::wstring (error.begin(), error.end())).c_str());

    // The mapping and the //c pointer nudge are applied HERE, past the last
    // refusal. The loader above can still reject the config, and until it has
    // not, the machine that keeps running is the one being left: applying them
    // earlier handed IT the mapping of a machine it never became.
    //
    // A null block -- no prefs, no store, a merge that failed, a merged
    // document that is not an object -- seeds from the legacy global setting,
    // the same fallback a machine that has never stored a mapping gets, rather
    // than leaving the mapping of the machine being left in place. This is the
    // launch path's rule (AdoptInputModeForMachine runs ahead of the bail in
    // ApplyPersistedAudioPrefs) applied to switching.
    //
    // State only -- this runs on the CPU thread, and the selector sync asserts
    // the UI thread. The post-switch reflow (WM_APP_DXUI_UPDATE_TITLE) puts it
    // on the chrome. The nudge follows the adopt, so the //c mouse default
    // sees the restored mapping rather than being overwritten by it.
    m_shell.AdoptInputModeForMachine (inputUiPrefs);
    m_shell.ApplyDefaultPointerForMachine();

    // Auto-flush every dirty disk before tearing down the previous
    // machine so user writes survive the machine switch.
    {
        HRESULT  hrFlush = m_shell.m_machine.GetDiskStore().FlushAll();
        IGNORE_RETURN_VALUE (hrFlush, S_OK);
    }

    // Snapshot the currently-mounted slot-6 disks so they follow the
    // user across the machine switch. The mental model is physical:
    // the user mounted a disk, changed the host machine, and expects
    // the disk to still be in the drive. Re-mounting on the new
    // machine also updates its per-machine prefs so the disk sticks
    // on subsequent launches. Empty paths fall through to the
    // per-machine prefs lookup inside MountCommandLineDisks.
    carryDisk1 = m_shell.m_machine.GetDiskStore().GetSourcePath (6, 0);
    carryDisk2 = m_shell.m_machine.GetDiskStore().GetSourcePath (6, 1);

    // Tear down current machine. The Disk II debug dialog (if open)
    // holds a raw pointer into the old CPU's cycle counter; revoke it
    // before the CPU is reset so the dialog can't dereference dangling
    // memory between here and CreateCpu below.
    if (m_shell.m_disk2DebugPanel != nullptr)
    {
        m_shell.m_disk2DebugPanel->SetCycleCounter (nullptr);
    }

    if (m_shell.m_inputDebugPanel != nullptr)
    {
        m_shell.m_inputDebugPanel->SetCycleCounter (nullptr);
    }

    if (m_shell.m_machine.GetRefs().keyboard != nullptr)
    {
        m_shell.m_machine.GetRefs().keyboard->SetInputEventSink (nullptr);
    }

    {
        auto * iieSwitches = m_shell.m_machine.GetRefs().iieSoftSwitches;
        if (iieSwitches != nullptr)
        {
            iieSwitches->SetInputEventSink (nullptr);
        }
    }

    if (m_shell.m_machine.GetRefs().gamePort != nullptr)
    {
        m_shell.m_machine.GetRefs().gamePort->SetInputEventSink (nullptr);
    }

    // Tear down ALL per-machine state in one atomic move. The refs are a
    // struct of observer pointers into the owning collections
    // (owned devices, video modes); resetting them as a whole keeps
    // the "every observer must be invalidated when its owner goes
    // away" invariant from rotting as new observers are added. The MMU
    // is a unique_ptr that survives across switches and is only
    // reassigned when the new config carries an apple2e-mmu device;
    // it must be explicitly reset here or it'll keep its stale
    // RamDevice pointer alive across a //e -> ][ switch.
    //
    // Stop the printer drain thread first: its job holds a reference into the
    // card's ring, which clearing the owned devices is about to free.
    m_shell.m_printerWorker.Stop();

    // Persist the outgoing machine's pending strip before its card is freed --
    // The host still holds the outgoing machine's name here (FR-026). An empty
    // strip clears any stale sidecar.
    if (!m_shell.m_machine.GetCurrentMachineName().empty())
    {
        PrinterJob *   printJob = m_shell.m_printerWorker.GetJob();

        if (printJob != nullptr && printJob->HasContent())
        {
            HRESULT   hrSave = PrintJobStore::Save (m_shell.GetPendingPrintDir(), printJob->GetRaster());
            IGNORE_RETURN_VALUE (hrSave, S_OK);
        }
        else
        {
            PrintJobStore::Clear (m_shell.GetPendingPrintDir());
        }
    }

    // The Mockingboard's PSG audio sources are owned by the card device
    // among the owned devices, so the mixer's borrowed pointers must be dropped
    // before that collection is cleared below (CreateMemoryDevices
    // re-registers fresh ones for the new machine).
    m_shell.m_mockingboardAudioMixer.UnregisterAllSources();

    // Reclaim IRQ source tokens before the devices that hold them are
    // destroyed; the rebuilt machine re-registers from a fresh pool.
    m_shell.m_machine.GetInterruptController().ResetSources();

    m_shell.m_machine.SetCpu (nullptr);
    // The //c ROM-bank coordinator holds references into the language card
    // (owned) + MMU; drop it before those owners are torn down.
    m_shell.m_machine.SetApple2cRomBank (nullptr);
    m_shell.m_machine.GetOwnedDevices().clear();
    m_shell.m_machine.GetVideoModes().clear();
    m_shell.m_machine.GetMemoryBus() = MemoryBus();
    m_shell.m_machine.GetRefs()      = {};
    m_shell.m_machine.SetMmu (nullptr);

    // Initialize with new config
    m_shell.m_machine.SetCurrentMachineName (machineName);
    m_shell.m_machine.GetConfig()             = newConfig;
    m_shell.m_cyclesPerFrame     = newConfig.cyclesPerFrame;

    //  The same build the initial launch runs. It used to be repeated here,
    //  which is how the //c's $C028 ROM banking came to be wired on a switch
    //  and not on a cold start -- the two copies drifted, and the machine
    //  that booted to a garbage screen was the one nobody switched to.
    hr = m_shell.BuildMachineDevices (newConfig);
    CHR (hr);

    // The build unregistered the old disk-audio sources and created new
    // ones. They are registered with the mixer but hold no sample data yet
    // -- SetMechanism is what triggers LoadSamples on each registered
    // source. Without this re-poke, the new machine's drive plays in eerie
    // silence (FR-009).
    {
        std::wstring  currentMechanism = m_shell.m_driveAudioMixer.GetMechanism();
        HRESULT       hrMech           = m_shell.m_driveAudioMixer.SetMechanism (currentMechanism);
        IGNORE_RETURN_VALUE (hrMech, S_OK);
    }

    // Re-attach the new CPU's cycle counter to the debug dialog (the
    // pointer was revoked above before the old CPU was destroyed).
    if (m_shell.m_disk2DebugPanel != nullptr && m_shell.m_machine.GetCpu() != nullptr)
    {
        m_shell.m_disk2DebugPanel->SetCycleCounter (m_shell.m_machine.GetCpu()->GetCycleCounterPtr());
    }

    if (m_shell.m_inputDebugPanel != nullptr && m_shell.m_machine.GetCpu() != nullptr)
    {
        m_shell.m_inputDebugPanel->SetCycleCounter (m_shell.m_machine.GetCpu()->GetCycleCounterPtr());
    }

    // Re-wire the debug dialog onto the freshly built controller +
    // audio source. Without this the dialog goes silent after a
    // machine switch even though it's still on screen.
    m_shell.AttachDebugSinksIfOpen();

    m_shell.UpdateWindowTitle();

    // Record the new active machine in GlobalUserPrefs so the next
    // launch boots it by default. SaveGlobalPrefs flushes the change
    // to UserPrefs.json on disk.
    if (m_shell.m_globalPrefs.lastSelectedMachine != machineNameNarrow)
    {
        m_shell.m_globalPrefs.lastSelectedMachine = machineNameNarrow;
        m_shell.SaveGlobalPrefs();
    }

    // Same cold-power-on sequence as Initialize() -- seed DRAM and
    // run the 6502 /RESET sequence. Without this the newly-built
    // machine starts with a random PC into uninitialized RAM. Mounts
    // persist across the switch (they were flushed above and re-
    // mounted by the new config); aux RAM, LC RAM, and CPU registers
    // are all reseeded.
    //
    // Must run BEFORE the per-machine remount: PowerCycle ejects every
    // drive and rebinds the controller's engine to its empty internal
    // disk, which would silently throw away whatever we just mounted.
    PowerCycle();

    // Remount per-machine disks if any were saved last time this
    // machine was active. The disks that were in the drives before
    // the switch take priority (passed explicitly here) so the user's
    // physical mental model holds: the disk in the drive stays in
    // the drive across a machine swap. Empty paths fall through
    // harmlessly so a never-used machine won't try to mount anything.
    //
    // If the new machine has no Disk II controller at slot 6 (future
    // non-Apple-II family), drop the carry rather than silently relying
    // on MountDiskInSlot6's nullptr CBR. The disk in DiskImageStore
    // was already flushed above, so no user data is lost.
    if (!m_shell.m_diskManager->HasSlot6Controller())
    {
        carryDisk1.clear();
        carryDisk2.clear();
    }

    m_shell.m_diskManager->MountCommandLineDisks (carryDisk1, carryDisk2);

    // Same rule as the color mode: a machine with no saved speed gets the
    // default, never the outgoing machine's.
    if (speedCmd == 0)
    {
        speedCmd = IDM_MACHINE_SPEED_1X;
    }

    if (speedCmd != 0)
    {
        // Same routing rule as the color command above: dispatch on the UI
        // thread via the message loop, never directly from the CPU thread.
        PostMessageW (m_shell.m_hwnd, WM_COMMAND, MAKEWPARAM (speedCmd, 0), 0);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoftReset
//
//  Drives the //e /RESET path: every device clears its reset-sensitive
//  state (80COL/ALTCHARSET no longer survive), the MMU returns to the
//  post-reset banking flags, and the CPU re-loads PC from $FFFC. User
//  RAM is preserved.
//
////////////////////////////////////////////////////////////////////////////////

void MachineManager::SoftReset()
{
    m_shell.m_machine.SoftReset();

    // Re-zero the Disk II Debug Uptime column on every reset so the user
    // sees a clean 00:00 anchor after each Ctrl+Shift+R / Ctrl+Shift+P.
    m_shell.ResetUptimeAnchor();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PowerCycle
//
////////////////////////////////////////////////////////////////////////////////

void MachineManager::PowerCycle()
{
    m_shell.m_machine.PowerCycle();

    m_shell.ResetUptimeAnchor();
}





