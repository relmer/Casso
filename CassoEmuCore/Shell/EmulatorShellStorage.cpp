#include "Pch.h"

#include "Shell/EmulatorShell.h"
#include "Shell/EmulatorShellInternal.h"
#include "Shell/TapeManager.h"
#include "Core/JsonParser.h"
#include "Core/PathResolver.h"
#include "Ui/Settings/SettingsPanelState.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::IsSecondDriveOffered
//
//  Whether this machine has anywhere to plug a second drive: the //c's disk
//  port, or the second connector of an enabled Disk ][ card.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::IsSecondDriveOffered() const
{
    const MachineConfig  & config = m_machine.GetConfig();



    if (config.systemRom.romBankSize != 0)
    {
        return true;
    }

    for (const SlotConfig & slot : config.slots)
    {
        if (slot.enabled && slot.device == kpszDiskIiDevice)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SetSecondDriveConnected
//
//  The same live change Settings makes -- the command ejects a disk from a
//  drive being taken away and relays the chrome -- and then saved, since no
//  sheet's OK is coming to save it.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetSecondDriveConnected (bool connected)
{
    if (!IsSecondDriveOffered() || connected == ShouldShowExternalDrive())
    {
        return;
    }

    HandleCommand (connected ? IDM_DRIVE_EXTERNAL_CONNECT : IDM_DRIVE_EXTERNAL_DISCONNECT);
    SaveStorageDevices();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SetTapeRecorderConnected
//
//  Disconnecting ejects the tape first, which writes back anything recorded
//  onto it: a recorder that is not there cannot be holding one. The recorder
//  goes from the drive band, the fullscreen strip and the desk scene alike,
//  which all ask IsTapeRecorderShown.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetTapeRecorderConnected (bool connected)
{
    if (!MachineHasCassettePort() || connected == m_tapeRecorderConnected)
    {
        return;
    }

    if (!connected && m_tapeManager != nullptr)
    {
        m_tapeManager->Eject();
    }

    m_tapeRecorderConnected = connected;

    ReflowChromeForMachineChange();
    SaveStorageDevices();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SaveStorageDevices
//
//  Writes which storage devices are connected into the machine's saved
//  settings, through the same document Settings writes: the second drive in
//  the machine's own port or its Disk ][ card's, and the recorder in
//  $cassoUiPrefs.
//
//  Read fresh from disk each time, so a Settings sheet open alongside is not
//  overwritten by a stale copy -- and Settings re-reads the drive state on OK
//  for the same reason.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SaveStorageDevices()
{
    HRESULT             hr                = S_OK;
    std::string         machineNameNarrow = GetCurrentMachineNameNarrow();
    std::wstring        configRelPath     = std::wstring (L"Machines\\") + m_machine.GetCurrentMachineName() +
                                            L"\\" + m_machine.GetCurrentMachineName() + L".json";
    fs::path            configPath        = PathResolver::FindFile (PathResolver::BuildSearchPaths (
                                                PathResolver::GetExecutableDirectory(),
                                                PathResolver::GetWorkingDirectory()),
                                                configRelPath);
    JsonValue           defaultJson;
    JsonValue           mergedJson;
    JsonValue           savedJson;
    JsonParseError      parseErr;
    std::ifstream       configFile;
    std::stringstream   ss;
    SettingsPanelState  state;



    BAIL_OUT_IF (m_userConfigStore == nullptr || configPath.empty(), S_OK);

    configFile.open (configPath);
    BAIL_OUT_IF (!configFile.good(), S_OK);

    ss << configFile.rdbuf();

    hr = JsonParser::Parse (ss.str(), defaultJson, parseErr);
    CHRA (hr);

    hr = m_userConfigStore->Load (machineNameNarrow, defaultJson, m_uiFs, mergedJson);
    CHR (hr);

    hr = state.LoadFromMachine (machineNameNarrow, defaultJson, mergedJson);
    CHR (hr);

    if (IsSecondDriveOffered())
    {
        state.SetSecondDriveAttached (ShouldShowExternalDrive());
    }

    state.SetTapeRecorderConnected (m_tapeRecorderConnected);

    savedJson = state.BuildCurrentJson();

    hr = m_userConfigStore->SaveDelta (machineNameNarrow, savedJson, defaultJson, m_uiFs);
    CHR (hr);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ShowStorageContextMenu
//
//  A device's own menu, from a right-click on it: what the Storage menu does
//  for that device, ending with the switch that disconnects it. Drive 1 has
//  no such switch; the machine's first drive is always there.
//
//  The rows are the Storage menu's own commands, so they read, enable and
//  check exactly as the menu's do.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ShowStorageContextMenu (int device, int x, int y)
{
    const EmulatorCommands          & commands = m_mainMenu.GetCommands();
    std::vector<DxuiPopupMenuItem>    items;
    std::vector<int>                  ids;



    if (m_host == nullptr)
    {
        return;
    }

    if (device == kStorageMenuRecorder)
    {
        ids = { IDM_TAPE_INSERT, IDM_TAPE_PLAY, IDM_TAPE_STOP, IDM_TAPE_REWIND, IDM_TAPE_FASTFORWARD,
                IDM_TAPE_EJECT, 0, IDM_STORAGE_RECORDER };
    }
    else if (device == 0)
    {
        ids = { IDM_DISK_INSERT1, IDM_DISK_EJECT1, IDM_DISK_WP1, IDM_DISK_SALVAGE1 };
    }
    else
    {
        ids = { IDM_DISK_INSERT2, IDM_DISK_EJECT2, IDM_DISK_WP2, IDM_DISK_SALVAGE2, 0, IDM_STORAGE_DRIVE2 };
    }

    for (int id : ids)
    {
        items.push_back (id == 0 ? DxuiPopupMenuItem::ForSeparator() : DxuiPopupMenuItem::ForCommand (commands.Find (id)));
    }

    DxuiContextMenu::Show (*m_host, x, y, std::move (items));
}
