#include "Pch.h"

#include "Shell/EmulatorShell.h"
#include "Shell/Components/ShellChrome.h"
#include "Config/UserConfigStore.h"
#include "Shell/Components/ShellSettings.h"
#include "Shell/DiskManager.h"
#include "Shell/EmulatorShellInternal.h"
#include "Shell/Components/ShellDisks.h"
#include "Shell/Components/ShellDeskScene.h"
#include "Shell/Components/ShellTapeDeck.h"
#include "Devices/Tape/TapeImageLoader.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Shell/TapeManager.h"
#include "Core/JsonParser.h"
#include "Core/PathResolver.h"
#include "Core/TextEncoding.h"
#include "Seams/IHostDialogs.h"
#include "Ui/Dialogs/DialogDefinition.h"
#include "Ui/Settings/SettingsPanelState.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::OnFileDropped
//
//  A file dropped on a drive mounts there and one dropped on the recorder is
//  inserted into it, but only the kind each takes: a tape dropped on a drive,
//  or a disk on the recorder, is ignored rather than mounted somewhere it
//  cannot be read.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnFileDropped (int tag, const std::wstring & path)
{
    if (tag == ShellTapeDeck::kTapeDropTag)
    {
        if (TapeImageLoader::IsTapeFileExtension (path))
        {
            m_tapeDeck->InsertTape (path);
        }
    }
    else if (tag >= ShellDeskScene::kBarDropTagBase)
    {
        const DriveBarFan  & fan   = m_chrome->m_driveBarFan.GetFan();
        int                  c     = (tag - ShellDeskScene::kBarDropTagBase) / ShellDeskScene::kBarDropClassStride;
        int                  entry = (tag - ShellDeskScene::kBarDropTagBase) % ShellDeskScene::kBarDropClassStride;



        if (entry < fan.GetEntryCount (c) && ShellDeskScene::IsFloppy35 (fan.GetDrive (c, entry)))
        {
            m_disks->Mount (fan.GetDrive (c, entry).slot, fan.GetDrive (c, entry).drive, path);
        }
        else if (entry < fan.GetEntryCount (c) && ShellDeskScene::IsHardDisk (fan.GetDrive (c, entry)))
        {
            ChangeHardDiskImage (fan.GetDrive (c, entry), path);
        }
    }
    else if (tag >= ShellDeskScene::kExtraDropTagBase)
    {
        m_scene->InsertInExtraDrive (tag - ShellDeskScene::kExtraDropTagBase, path);
    }
    else if (IsSupportedDiskImageExtension (path))
    {
        m_disks->Mount (6, tag, path);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SetSecondDriveConnected
//
//  Makes the same live change Settings makes (the command ejects a disk from
//  a drive being removed and lays out the chrome again), then saves it, since
//  no Settings OK will follow to save it.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetSecondDriveConnected (bool connected)
{
    if (!m_disks->IsSecondDriveOffered() || connected == m_disks->ShouldShowExternalDrive())
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
//  onto it, since a disconnected recorder has no tape. The recorder is
//  removed from the drive band, the fullscreen strip and the desk scene
//  alike, which all check IsTapeRecorderShown.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetTapeRecorderConnected (bool connected)
{
    if (!m_tapeDeck->MachineHasCassettePort() || connected == m_tapeDeck->IsRecorderConnected())
    {
        return;
    }

    if (!connected && m_tapeDeck->GetManager() != nullptr)
    {
        m_tapeDeck->GetManager()->Eject();
    }

    m_tapeDeck->SetRecorderConnected (connected);

    m_chrome->ReflowChromeForMachineChange();
    SaveStorageDevices();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SaveStorageDevices
//
//  Writes which storage devices are connected into the machine's saved
//  settings: the second drive in the machine's port or its Disk ][ card's,
//  and the recorder in $cassoUiPrefs.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SaveStorageDevices()
{
    SaveMachineSettings ([this] (SettingsPanelState & state)
    {
        if (m_disks->IsSecondDriveOffered())
        {
            state.SetSecondDriveAttached (m_disks->ShouldShowExternalDrive());
        }

        state.SetTapeRecorderConnected (m_tapeDeck->IsRecorderConnected());
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SaveHardDiskImage
//
//  Writes a hard disk's new image into the machine's saved settings, where
//  the Machine tab has it.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SaveHardDiskImage (int slot, int drive, const std::string & path)
{
    SaveMachineSettings ([slot, drive, &path] (SettingsPanelState & state)
    {
        HRESULT  hr = state.SetSlotPortImage (slot, (size_t) drive, path);



        IGNORE_RETURN_VALUE (hr, S_OK);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::MakeHardDiskChangeDialog
//
////////////////////////////////////////////////////////////////////////////////

DialogDefinition EmulatorShell::MakeHardDiskChangeDialog (const std::wstring & driveTitle, const std::wstring & fileName)
{
    DialogDefinition  def;



    def.title = L"Change hard disk image";
    def.icon  = DialogIcon::Warning;
    def.body.push_back (DialogTextRun { std::format (L"Changing the image in {} to {} restarts the machine now.", driveTitle, fileName), false, std::wstring() });
    def.body.push_back (DialogTextRun { L"Anything the running program hasn't saved is lost.", false, std::wstring() });
    def.buttons.push_back (DialogButton { L"Restart", kHardDiskChangeRestart, true,  false, false });
    def.buttons.push_back (DialogButton { L"Cancel",  0,                      false, true,  false });

    return def;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ChangeHardDiskImage
//
//  UI thread. On Restart the image is attached on the CPU thread, which
//  restarts the machine once it is attached; on Cancel nothing changes.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ChangeHardDiskImage (const DeskDrive & drive, const std::wstring & path)
{
    int  choice = 0;



    m_chrome->HideTooltips();

    choice = ShowModalDialog (MakeHardDiskChangeDialog (ShellDeskScene::GetDriveTitle (drive), fs::path (path).filename().wstring()));

    if (choice == kHardDiskChangeRestart)
    {
        PostCommand (IDM_HARDDISK_ATTACH, std::format ("{} {} {}", drive.slot, drive.drive, TextEncoding::WideToUtf8 (path)));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::MakePortDriveDialog
//
////////////////////////////////////////////////////////////////////////////////

DialogDefinition EmulatorShell::MakePortDriveDialog (const std::string & device, bool plugIn)
{
    DialogDefinition  def;
    const wchar_t  *  kind = L"a UniDisk 3.5";



    if (device == kpszApple35DriveDevice)
    {
        kind = L"a 3.5-inch drive";
    }
    else if (device == kpszDiskIicDevice)
    {
        kind = L"a 5.25-inch drive";
    }

    def.title = plugIn ? L"Plug in a drive" : L"Unplug a drive";
    def.icon  = DialogIcon::Warning;
    def.body.push_back (DialogTextRun { std::format (L"{} {} restarts the machine now.", plugIn ? L"Plugging in" : L"Unplugging", kind), false, std::wstring() });
    def.body.push_back (DialogTextRun { L"Anything the running program hasn't saved is lost.", false, std::wstring() });
    def.buttons.push_back (DialogButton { L"Restart", kHardDiskChangeRestart, true,  false, false });
    def.buttons.push_back (DialogButton { L"Cancel",  0,                      false, true,  false });

    return def;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ChangePortDrive
//
//  The chain is saved with the machine, and the machine is rebuilt from it
//  on the CPU thread, as the Settings sheet's machine switch does.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ChangePortDrive (const std::string & device, bool plugIn, size_t nth)
{
    int  choice = 0;



    m_chrome->HideTooltips();

    choice = ShowModalDialog (MakePortDriveDialog (device, plugIn));

    if (choice == kHardDiskChangeRestart)
    {
        SaveMachineSettings ([&device, plugIn, nth] (SettingsPanelState & state)
        {
            if (plugIn)
            {
                state.AddDiskPortDrive (device);
            }
            else
            {
                state.RemoveDiskPortDrive (device, nth);
            }
        });

        PostCommand (IDM_FILE_OPEN, GetCurrentMachineNameNarrow());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::AddPortDriveRows
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::AddPortDriveRows (const std::string & device, int nth, std::vector<DxuiPopupMenuItem> & items)
{
    const EmulatorCommands  & commands = m_chrome->m_mainMenu.GetCommands();



    items.push_back (DxuiPopupMenuItem::ForSeparator());
    items.push_back (DxuiPopupMenuItem::ForCommand (commands.Find (IDM_STORAGE_ATTACH_APPLE35)));
    items.push_back (DxuiPopupMenuItem::ForCommand (commands.Find (IDM_STORAGE_ATTACH_FLOPPY)));

    if (nth >= 0)
    {
        auto  unplug = std::make_shared<DxuiCommand>();



        unplug->label    = L"&Unplug drive";
        unplug->dispatch = [this, device, nth] () { ChangePortDrive (device, false, (size_t) nth); };

        items.push_back (DxuiPopupMenuItem::ForCommand (unplug));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::BrowseHardDisk
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::BrowseHardDisk (const DeskDrive & drive)
{
    HRESULT         hr     = S_OK;
    FileDialogSpec  spec;
    fs::path        chosen;
    bool            picked = false;



    spec.filters = { { L"Hard disk image", L"*.po;*.hdv;*.2mg;*.2img" }, { L"All files", L"*.*" } };

    m_chrome->HideTooltips();

    hr = m_hostDialogs.PickFileToOpen (m_hwnd, spec, chosen, picked);
    CHR (hr);
    BAIL_OUT_IF (!picked, S_OK);

    ChangeHardDiskImage (drive, chosen.wstring());

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SaveMachineSettings
//
//  Applies `edit` to the machine's saved settings, through the same document
//  Settings writes.
//
//  Read fresh from disk each time, so a Settings sheet open alongside is not
//  overwritten by a stale copy -- and Settings re-reads the drive state on OK
//  for the same reason.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SaveMachineSettings (const std::function<void (SettingsPanelState &)> & edit)
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



    BAIL_OUT_IF (m_settings->GetConfigStore() == nullptr || configPath.empty(), S_OK);

    configFile.open (configPath);
    BAIL_OUT_IF (!configFile.good(), S_OK);

    ss << configFile.rdbuf();

    hr = JsonParser::Parse (ss.str(), defaultJson, parseErr);
    CHRA (hr);

    hr = m_settings->GetConfigStore()->Load (machineNameNarrow, defaultJson, m_settings->GetFileSystem(), mergedJson);
    CHR (hr);

    hr = state.LoadFromMachine (machineNameNarrow, defaultJson, mergedJson);
    CHR (hr);

    edit (state);

    savedJson = state.BuildCurrentJson();

    hr = m_settings->GetConfigStore()->SaveDelta (machineNameNarrow, savedJson, defaultJson, m_settings->GetFileSystem());
    CHR (hr);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ShowStorageContextMenu
//
//  Shows a device's own menu on a right-click: the switch that connects it,
//  then the Storage menu's commands for that device. Drive 1 has no such
//  switch; the machine's first drive is always present.
//
//  The rows are the Storage menu's own commands, so their text, enabled
//  state and check marks match the menu's exactly.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ShowStorageContextMenu (int device, int x, int y)
{
    const EmulatorCommands          & commands = m_chrome->m_mainMenu.GetCommands();
    std::vector<DxuiPopupMenuItem>    items;
    std::vector<int>                  ids;



    if (m_host == nullptr)
    {
        return;
    }

    if (device >= kStorageMenuBar)
    {
        const DriveBarFan  & fan   = m_chrome->m_driveBarFan.GetFan();
        int                  c     = (device - kStorageMenuBar) / ShellDeskScene::kBarDropClassStride;
        int                  entry = (device - kStorageMenuBar) % ShellDeskScene::kBarDropClassStride;



        if (entry < fan.GetEntryCount (c))
        {
            ShowDriveContextMenu (fan.GetDrive (c, entry), x, y);
        }

        return;
    }

    if (device >= kStorageMenuDeskExtra)
    {
        if (device - kStorageMenuDeskExtra < m_scene->m_deskScene.GetExtraCount())
        {
            ShowDriveContextMenu (m_scene->m_deskScene.GetExtraDrive (device - kStorageMenuDeskExtra), x, y);
        }

        return;
    }

    if (device == kStorageMenuRecorder)
    {
        ids = { IDM_STORAGE_RECORDER, 0, IDM_TAPE_INSERT, IDM_TAPE_PLAY, IDM_TAPE_STOP, IDM_TAPE_REWIND,
                IDM_TAPE_FASTFORWARD, IDM_TAPE_EJECT };
    }
    else if (device == 0)
    {
        // Drive 1 is always there, so its menu is where a detached drive 2 or
        // recorder is attached again: neither is on screen to be clicked.
        bool  drive2Away   = m_disks->IsSecondDriveOffered() && !m_disks->ShouldShowExternalDrive();
        bool  recorderAway = m_tapeDeck->MachineHasCassettePort() && !m_tapeDeck->IsTapeRecorderShown();

        ids = { IDM_DISK_INSERT1, IDM_DISK_EJECT1, IDM_DISK_WP1, IDM_DISK_SALVAGE1, IDM_DISK_INSPECT1, IDM_DISK_THUMBNAIL1 };

        if (drive2Away || recorderAway)
        {
            ids.push_back (0);
        }

        if (drive2Away)
        {
            ids.push_back (IDM_STORAGE_DRIVE2);
        }

        if (recorderAway)
        {
            ids.push_back (IDM_STORAGE_RECORDER);
        }
    }
    else
    {
        ids = { IDM_STORAGE_DRIVE2, 0, IDM_DISK_INSERT2, IDM_DISK_EJECT2, IDM_DISK_WP2, IDM_DISK_SALVAGE2, IDM_DISK_INSPECT2, IDM_DISK_THUMBNAIL2 };
    }

    for (int id : ids)
    {
        items.push_back (id == 0 ? DxuiPopupMenuItem::ForSeparator() : DxuiPopupMenuItem::ForCommand (commands.Find (id)));
    }

    ids.clear();

    // On the //c Plus the 5.25-inch drives are on the disk port's chain. Only
    // the last one is unplugged, since a disk is saved by its drive's number.
    if (m_disks->IsPortChainOffered() && (device == 0 || device == 1))
    {
        int  floppies = m_disks->CountPortDrives (kpszDiskIicDevice);



        AddPortDriveRows (kpszDiskIicDevice, (device >= 0 && device == floppies - 1) ? device : -1, items);
    }

    // Every device's menu ends with the auto-hide switch, since the drives and
    // the recorder appear and disappear together.
    ids.push_back (0);
    ids.push_back (IDM_VIEW_AUTOHIDE_DRIVES);
    ids.push_back (IDM_VIEW_FLOPPIES_ON_DESK);
    ids.push_back (IDM_VIEW_HARD_DISKS_ON_DESK);
    ids.push_back (IDM_VIEW_FLOPPY_PAIR_BESIDE);

    for (int id : ids)
    {
        items.push_back (id == 0 ? DxuiPopupMenuItem::ForSeparator() : DxuiPopupMenuItem::ForCommand (commands.Find (id)));
    }

    DxuiContextMenu::Show (*m_host, x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ShowDriveContextMenu
//
//  A 3.5-inch drive's or hard disk's menu: the drive's name, then for a
//  3.5-inch drive its insert and eject, and for a hard disk its image
//  change, then the switches every storage menu ends with.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ShowDriveContextMenu (const DeskDrive & drive, int x, int y)
{
    const EmulatorCommands          & commands = m_chrome->m_mainMenu.GetCommands();
    std::vector<DxuiPopupMenuItem>    items;



    if (m_host == nullptr)
    {
        return;
    }

    items.push_back (DxuiPopupMenuItem::ForHeader (ShellDeskScene::GetDriveTitle (drive)));

    if (ShellDeskScene::IsFloppy35 (drive))
    {
        auto  insert = std::make_shared<DxuiCommand>();
        auto  eject  = std::make_shared<DxuiCommand>();



        insert->label     = L"&Insert disk...";
        insert->dispatch  = [this, drive] () { m_scene->BrowseFloppy35 (drive); };
        eject->label      = L"&Eject disk";
        eject->dispatch   = [this, drive] () { m_disks->Eject (drive.slot, drive.drive); };
        eject->isEnabled  = [this, drive] () { return !m_scene->GetDriveImagePath (drive).empty(); };

        items.push_back (DxuiPopupMenuItem::ForCommand (insert));
        items.push_back (DxuiPopupMenuItem::ForCommand (eject));
    }
    else if (ShellDeskScene::IsHardDisk (drive))
    {
        auto  change = std::make_shared<DxuiCommand>();



        change->label    = L"&Change image...";
        change->dispatch = [this, drive] () { BrowseHardDisk (drive); };

        items.push_back (DxuiPopupMenuItem::ForCommand (change));
    }

    // On the //c Plus, the drives behind the built-in one are on the disk
    // port's chain, the built-in drive taking the first desk index.
    if (drive.slot == 0 && m_disks->IsPortChainOffered())
    {
        std::string  device;
        int          nth    = -1;



        if (drive.model == DeskDriveModel::Apple35Drive)
        {
            device = kpszApple35DriveDevice;
        }
        else if (drive.model == DeskDriveModel::UniDisk35)
        {
            device = kpszUniDisk35Device;
        }

        for (const PortConfig & port : m_machine.GetConfig().ports)
        {
            for (size_t i = 0; !device.empty() && i < port.chain.size() && (int) i < drive.drive; i++)
            {
                nth += (port.chain[i].device == device) ? 1 : 0;
            }
        }

        AddPortDriveRows (device, nth, items);
    }

    items.push_back (DxuiPopupMenuItem::ForSeparator());

    for (int id : { IDM_VIEW_AUTOHIDE_DRIVES, IDM_VIEW_FLOPPIES_ON_DESK, IDM_VIEW_HARD_DISKS_ON_DESK, IDM_VIEW_FLOPPY_PAIR_BESIDE })
    {
        items.push_back (DxuiPopupMenuItem::ForCommand (commands.Find (id)));
    }

    DxuiContextMenu::Show (*m_host, x, y, std::move (items));
}
