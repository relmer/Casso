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

DialogDefinition EmulatorShell::MakePortDriveDialog (PortDriveChange change, const std::string & device)
{
    DialogDefinition  def;
    std::wstring      action;



    switch (change)
    {
        case PortDriveChange::PlugIn:
            def.title = L"Plug in a drive";
            action    = std::format (L"Plugging in {}", GetPortDriveText (device));
            break;

        case PortDriveChange::Unplug:
            def.title = L"Unplug a drive";
            action    = std::format (L"Unplugging {}", GetPortDriveText (device));
            break;

        case PortDriveChange::Swap:
            def.title = L"Swap a drive";
            action    = std::format (L"Swapping {} for {}", GetPortDriveText (device), GetPortDriveText (ShellDisks::GetSwapDevice (device)));
            break;
    }

    def.icon  = DialogIcon::Warning;
    def.body.push_back (DialogTextRun { action + L" restarts the machine now.", false, std::wstring() });
    def.body.push_back (DialogTextRun { L"Anything the running program hasn't saved is lost.", false, std::wstring() });
    def.buttons.push_back (DialogButton { L"Restart", kHardDiskChangeRestart, true,  false, false });
    def.buttons.push_back (DialogButton { L"Cancel",  0,                      false, true,  false });

    return def;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::GetPortDriveText
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * EmulatorShell::GetPortDriveText (const std::string & device)
{
    const wchar_t  * text = L"a UniDisk 3.5";



    if (device == kpszApple35DriveDevice)
    {
        text = L"a 3.5-inch drive";
    }
    else if (device == kpszDiskIicDevice)
    {
        text = L"a 5.25-inch drive";
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ChangePortDrive
//
//  The chain is saved with the machine, and the machine is rebuilt from it
//  on the CPU thread, as the Settings sheet's machine switch does. A swap
//  removes the drive and plugs in one of the other size, which goes where
//  the firmware looks for its kind.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ChangePortDrive (PortDriveChange change, const std::string & device, size_t nth)
{
    int  choice = 0;



    m_chrome->HideTooltips();

    choice = ShowModalDialog (MakePortDriveDialog (change, device));

    if (choice == kHardDiskChangeRestart)
    {
        SaveMachineSettings ([&device, change, nth] (SettingsPanelState & state)
        {
            if (change != PortDriveChange::PlugIn)
            {
                state.RemoveDiskPortDrive (device, nth);
            }

            if (change == PortDriveChange::PlugIn)
            {
                state.AddDiskPortDrive (device);
            }
            else if (change == PortDriveChange::Swap)
            {
                state.AddDiskPortDrive (ShellDisks::GetSwapDevice (device));
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

    if (nth >= 0)
    {
        auto  unplug = std::make_shared<DxuiCommand>();



        unplug->label    = L"&Unplug drive";
        unplug->dispatch = [this, device, nth] () { ChangePortDrive (PortDriveChange::Unplug, device, (size_t) nth); };

        items.push_back (DxuiPopupMenuItem::ForCommand (unplug));

        if (!ShellDisks::GetSwapDevice (device).empty())
        {
            auto  swap = std::make_shared<DxuiCommand>();



            swap->label     = (device == kpszApple35DriveDevice) ? L"S&wap for a 5.25-inch drive" : L"S&wap for a 3.5-inch drive";
            swap->dispatch  = [this, device, nth] () { ChangePortDrive (PortDriveChange::Swap, device, (size_t) nth); };
            swap->isVisible = [this, device] () { return m_disks->CanSwapPortDrive (device); };

            items.push_back (DxuiPopupMenuItem::ForCommand (swap));
        }
    }

    items.push_back (DxuiPopupMenuItem::ForCommand (commands.Find (IDM_STORAGE_ATTACH_APPLE35)));
    items.push_back (DxuiPopupMenuItem::ForCommand (commands.Find (IDM_STORAGE_ATTACH_FLOPPY)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::MakeRelabeled
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiCommand> EmulatorShell::MakeRelabeled (const std::shared_ptr<const DxuiCommand> & command, const wchar_t * label)
{
    std::shared_ptr<DxuiCommand>  copy;



    if (command == nullptr)
    {
        return command;
    }

    copy            = std::make_shared<DxuiCommand> (*command);
    copy->label     = label;
    copy->labelText = nullptr;

    return copy;
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
//  A device's right-click menu. A drive's is the floppy menu
//  (ShowFloppyContextMenu) or a hard disk's; the recorder's is the switch
//  that connects it, then the Storage menu's tape commands, so their
//  enabled state matches the menu's.
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

    if (device != kStorageMenuRecorder)
    {
        ShowFloppyContextMenu (m_scene->GetDiskIiDrive (device), x, y);
        return;
    }

    ids = { IDM_STORAGE_RECORDER, 0, IDM_TAPE_INSERT, IDM_TAPE_PLAY, IDM_TAPE_STOP, IDM_TAPE_REWIND,
            IDM_TAPE_FASTFORWARD, IDM_TAPE_EJECT };

    for (int id : ids)
    {
        items.push_back (id == 0 ? DxuiPopupMenuItem::ForSeparator() : DxuiPopupMenuItem::ForCommand (commands.Find (id)));
    }

    ids.clear();

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
//  EmulatorShell::ShowFloppyContextMenu
//
//  The drive's name, the disk's commands, the drive's place on the port, the
//  rows that attach a detached drive or recorder, then the switches every
//  storage menu ends with. Rows that don't apply are left out: the Disk II
//  commands on a 3.5-inch drive, "Unplug drive" on a built-in drive, and
//  the plug-in and swap rows the port has no room for.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ShowFloppyContextMenu (const DeskDrive & drive, int x, int y)
{
    const EmulatorCommands          & commands = m_chrome->m_mainMenu.GetCommands();
    std::vector<DxuiPopupMenuItem>    items;
    bool                              isDiskIi = (drive.model == DeskDriveModel::DiskII || drive.model == DeskDriveModel::Disk2c);
    bool                              isFirst  = isDiskIi && drive.drive == 0;
    std::string                       device;
    int                               nth      = -1;



    if (m_host == nullptr)
    {
        return;
    }

    items.push_back (DxuiPopupMenuItem::ForHeader (ShellDeskScene::GetDriveTitle (drive)));

    // A machine's second Disk II is connected and disconnected here.
    if (isDiskIi && !isFirst)
    {
        items.push_back (DxuiPopupMenuItem::ForCommand (commands.Find (IDM_STORAGE_DRIVE2)));
        items.push_back (DxuiPopupMenuItem::ForSeparator());
    }

    if (isDiskIi)
    {
        items.push_back (DxuiPopupMenuItem::ForCommand (MakeRelabeled (commands.Find (isFirst ? IDM_DISK_INSERT1    : IDM_DISK_INSERT2),    L"&Insert disk...")));
        items.push_back (DxuiPopupMenuItem::ForCommand (MakeRelabeled (commands.Find (isFirst ? IDM_DISK_EJECT1     : IDM_DISK_EJECT2),     L"&Eject disk")));
        items.push_back (DxuiPopupMenuItem::ForCommand (commands.Find (isFirst ? IDM_DISK_WP1      : IDM_DISK_WP2)));
        items.push_back (DxuiPopupMenuItem::ForCommand (commands.Find (isFirst ? IDM_DISK_SALVAGE1 : IDM_DISK_SALVAGE2)));
        items.push_back (DxuiPopupMenuItem::ForCommand (MakeRelabeled (commands.Find (isFirst ? IDM_DISK_INSPECT1   : IDM_DISK_INSPECT2),   L"Inspect &disk...")));
        items.push_back (DxuiPopupMenuItem::ForCommand (MakeRelabeled (commands.Find (isFirst ? IDM_DISK_THUMBNAIL1 : IDM_DISK_THUMBNAIL2), L"Save screenshot as the disk's thu&mbnail")));
    }
    else
    {
        auto  insert = std::make_shared<DxuiCommand>();
        auto  eject  = std::make_shared<DxuiCommand>();



        insert->label    = L"&Insert disk...";
        insert->dispatch = [this, drive] () { m_scene->BrowseFloppy35 (drive); };
        eject->label     = L"&Eject disk";
        eject->dispatch  = [this, drive] () { m_disks->Eject (drive.slot, drive.drive); };
        eject->isEnabled = [this, drive] () { return !m_scene->GetDriveImagePath (drive).empty(); };

        items.push_back (DxuiPopupMenuItem::ForCommand (insert));
        items.push_back (DxuiPopupMenuItem::ForCommand (eject));
    }

    if (m_disks->IsPortChainOffered())
    {
        FindPortDrive (drive, device, nth);
        AddPortDriveRows (device, nth, items);
    }

    // Drive 1 is always there, so its menu is where a detached drive 2 or
    // recorder is attached again: neither is on screen to be clicked.
    items.push_back (DxuiPopupMenuItem::ForSeparator());

    if (isFirst && m_disks->IsSecondDriveOffered() && !m_disks->ShouldShowExternalDrive())
    {
        items.push_back (DxuiPopupMenuItem::ForCommand (commands.Find (IDM_STORAGE_DRIVE2)));
    }

    if (isFirst && m_tapeDeck->MachineHasCassettePort() && !m_tapeDeck->IsTapeRecorderShown())
    {
        items.push_back (DxuiPopupMenuItem::ForCommand (commands.Find (IDM_STORAGE_RECORDER)));
    }

    items.push_back (DxuiPopupMenuItem::ForSeparator());

    for (int id : { IDM_VIEW_AUTOHIDE_DRIVES, IDM_VIEW_FLOPPIES_ON_DESK, IDM_VIEW_HARD_DISKS_ON_DESK, IDM_VIEW_FLOPPY_PAIR_BESIDE })
    {
        items.push_back (DxuiPopupMenuItem::ForCommand (commands.Find (id)));
    }

    DxuiContextMenu::Show (*m_host, x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::FindPortDrive
//
//  The //c Plus's 5.25-inch drives are drives 1 and 2 of slot 6, and only
//  the last one is unplugged, since a disk is saved by its drive's number.
//  The 3.5-inch drives behind the built-in one are on the chain, the
//  built-in drive taking the first desk index.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::FindPortDrive (const DeskDrive & drive, std::string & outDevice, int & outNth) const
{
    outDevice.clear();
    outNth = -1;

    if (drive.model == DeskDriveModel::DiskII || drive.model == DeskDriveModel::Disk2c)
    {
        outDevice = kpszDiskIicDevice;
        outNth    = (drive.drive == m_disks->CountPortDrives (kpszDiskIicDevice) - 1) ? drive.drive : -1;
        return;
    }

    if (drive.model == DeskDriveModel::Apple35Drive)
    {
        outDevice = kpszApple35DriveDevice;
    }
    else if (drive.model == DeskDriveModel::UniDisk35)
    {
        outDevice = kpszUniDisk35Device;
    }

    for (const PortConfig & port : m_machine.GetConfig().ports)
    {
        for (size_t i = 0; !outDevice.empty() && i < port.chain.size() && (int) i < drive.drive; i++)
        {
            outNth += (port.chain[i].device == outDevice) ? 1 : 0;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ShowDriveContextMenu
//
//  A drive's menu from the desk or the drive bar. A floppy drive's is the
//  floppy menu; a hard disk's is its name, its image change, then the
//  switches every storage menu ends with.
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

    if (ShellDeskScene::IsFloppy35 (drive) || drive.model == DeskDriveModel::DiskII || drive.model == DeskDriveModel::Disk2c)
    {
        ShowFloppyContextMenu (drive, x, y);
        return;
    }

    items.push_back (DxuiPopupMenuItem::ForHeader (ShellDeskScene::GetDriveTitle (drive)));

    if (ShellDeskScene::IsHardDisk (drive))
    {
        auto  change = std::make_shared<DxuiCommand>();



        change->label    = L"&Change image...";
        change->dispatch = [this, drive] () { BrowseHardDisk (drive); };

        items.push_back (DxuiPopupMenuItem::ForCommand (change));
    }

    items.push_back (DxuiPopupMenuItem::ForSeparator());

    for (int id : { IDM_VIEW_AUTOHIDE_DRIVES, IDM_VIEW_FLOPPIES_ON_DESK, IDM_VIEW_HARD_DISKS_ON_DESK, IDM_VIEW_FLOPPY_PAIR_BESIDE })
    {
        items.push_back (DxuiPopupMenuItem::ForCommand (commands.Find (id)));
    }

    DxuiContextMenu::Show (*m_host, x, y, std::move (items));
}
