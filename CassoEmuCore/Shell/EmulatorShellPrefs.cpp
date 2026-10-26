#include "Pch.h"
#include "Shell/Components/ShellInput.h"

#include "Shell/EmulatorShell.h"
#include "Shell/Components/ShellRenderer.h"
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
#include "Shell/ControllerInputThread.h"
#include "Machines/MachineDefinition.h"
#include "Ui/ThemeManager.h"





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
    m_renderer->SetColorMonitorTextArgbLive (
        ColorUtil::ResolveColorMonitorTextArgb (m_settings->GetPrefs().colorMonitorTextMode,
                                                m_settings->GetPrefs().colorMonitorTextCustomArgb));
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
    m_renderer->SetColorModeLive (MonitorCatalog::GetSettingsIndex (MonitorCatalog::GetColorModeForMachineJson (doc)));

    // The players' modes are global and apply to a machine with no block of
    // its own too, so the Joyport is settled above the guard below.
    m_input->MigrateJoyportAtLaunch (uiPrefs);

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
        if      (speedMode == "authentic") { m_cpuManager.SetSpeedMode (SpeedMode::Authentic, SpeedChooser::User); }
        else if (speedMode == "double")    { m_cpuManager.SetSpeedMode (SpeedMode::Double,    SpeedChooser::User); }
        else if (speedMode == "maximum")   { m_cpuManager.SetSpeedMode (SpeedMode::Maximum,   SpeedChooser::User); }
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

        RefreshMachineChromeFacts();
    }

    hrOpt = uiPrefs->GetBool ("mouseConnected", mouseConn);
    if (SUCCEEDED (hrOpt))
    {
        m_input->SetMouseConnected (mouseConn);
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
