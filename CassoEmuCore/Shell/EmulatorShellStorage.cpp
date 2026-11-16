#include "Pch.h"

#include "Shell/EmulatorShell.h"
#include "Shell/Components/ShellDrivePickers.h"
#include "Shell/Components/ShellChrome.h"
#include "Config/UserConfigStore.h"
#include "Shell/Components/ShellSettings.h"
#include "Shell/DiskManager.h"
#include "Shell/EmulatorShellInternal.h"
#include "Shell/Components/ShellDisks.h"
#include "Shell/Components/ShellDeskScene.h"
#include "Shell/Components/ShellTapeDeck.h"
#include "Devices/Tape/TapeImageLoader.h"
#include "Devices/Disk/DriveImageRule.h"
#include "Devices/Disk/ImageClassifier.h"
#include "Shell/BootDriveRule.h"
#include "Shell/DiskMenuRule.h"
#include "Shell/DiskPortChainRule.h"
#include "Shell/DriveDropRule.h"
#include "Shell/Layout/DriveLabel.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Ui/Settings/SlotCards.h"
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
//  inserted into it, when DriveImageRule passes the image for that drive.
//  Any other drop does nothing: no mount, no eject and no message, since the
//  cursor over that drive was the no-drop cursor.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnFileDropped (int tag, const std::wstring & path)
{
    ReceivingDrive  drive;
    DriveAddress    address;
    ImageClass      image    = ImageClassifier::Classify (m_machine.GetBlockMediaFileIo(), TextEncoding::WideToUtf8 (path));
    bool            hasDrive = TryGetDropDrive (tag, drive, address);
    bool            accepted = hasDrive && DriveImageRule::Accepts (drive, image);



    if (accepted)
    {
        InsertInDrive (address, drive.kind, GetDropDriveLabel (tag), path);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::GetDropDriveLabel
//
//  The drive under a drop tag, as the staged-image infobar states it.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring EmulatorShell::GetDropDriveLabel (int tag)
{
    const DriveBarFan  & fan       = m_chrome->m_driveBarFan.GetFan();
    int                  index     = 0;
    int                  barClass  = 0;
    int                  entry     = 0;
    bool                 isExtra   = DriveDropRule::TryGetExtraIndex (tag, index);
    bool                 isBar     = DriveDropRule::TryGetBarEntry (tag, barClass, entry);



    if (isExtra && index < m_scene->m_deskScene.GetExtraCount())
    {
        return DriveLabel::GetTitle (m_scene->m_deskScene.GetExtraDrive (index), true);
    }

    if (isBar && barClass < fan.GetClassCount() && entry < fan.GetEntryCount (barClass))
    {
        return DriveLabel::GetTitle (fan.GetDrive (barClass, entry), true);
    }

    return std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InsertInDrive
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::InsertInDrive (const DriveAddress & address, ReceivingDriveKind kind, const std::wstring & label, const std::wstring & path)
{
    HRESULT  hr = S_OK;



    if (address.isRecorder)
    {
        m_tapeDeck->InsertTape (path);
        return true;
    }

    //  A hard disk's image takes a restart, which is confirmed first.
    if (kind == ReceivingDriveKind::HardDisk)
    {
        ChangeHardDiskImage (address.slot, address.drive, label, path);
        return true;
    }

    hr = m_disks->Mount (address.slot, address.drive, path);

    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::RunModalPicker
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::RunModalPicker (HWND owner, PickerRequest request, PickerOutcome & outOutcome)
{
    return m_drivePickers->RunModal (owner, std::move (request), outOutcome);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::GetHardDiskBlocks
//
////////////////////////////////////////////////////////////////////////////////

uint32_t EmulatorShell::GetHardDiskBlocks (int slot, int port) const
{
    for (const SlotConfig & config : m_machine.GetConfig().slots)
    {
        if (config.slot == slot && config.enabled && port >= 0 && (size_t) port < config.ports.size())
        {
            const BlockDriveSpec *  drive = BlockDeviceCatalog::FindDrive (config.ports[(size_t) port].device);

            return (drive != nullptr) ? drive->maxBlocks : 0;
        }
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::TryGetDropDrive
//
//  The drive under a drop tag: a Disk II drive, the recorder, or a drive on
//  the desk or the drive bar that takes a disk while the machine runs.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::TryGetDropDrive (int tag, ReceivingDrive & outDrive, DriveAddress & outAddress)
{
    const DriveBarFan  & fan       = m_chrome->m_driveBarFan.GetFan();
    int                  index     = 0;
    int                  barClass  = 0;
    int                  entry     = 0;
    bool                 isExtra   = DriveDropRule::TryGetExtraIndex (tag, index);
    bool                 isBar     = DriveDropRule::TryGetBarEntry (tag, barClass, entry);
    const DeskDrive    * deskDrive = nullptr;



    outDrive   = ReceivingDrive();
    outAddress = DriveAddress();

    if (DriveDropRule::IsRecorderTag (tag))
    {
        outDrive.kind         = ReceivingDriveKind::Recorder;
        outAddress.isRecorder = true;
        return true;
    }

    if (DriveDropRule::IsDiskIiTag (tag))
    {
        outDrive.kind    = ReceivingDriveKind::Floppy525;
        outAddress.slot  = DriveDropRule::kDiskIiSlot;
        outAddress.drive = tag;
        return true;
    }

    if (isExtra && index < m_scene->m_deskScene.GetExtraCount())
    {
        deskDrive = &m_scene->m_deskScene.GetExtraDrive (index);
    }
    else if (isBar && barClass < fan.GetClassCount() && entry < fan.GetEntryCount (barClass))
    {
        deskDrive = &fan.GetDrive (barClass, entry);
    }

    if (deskDrive == nullptr)
    {
        return false;
    }

    outAddress.slot  = deskDrive->slot;
    outAddress.drive = deskDrive->drive;

    return DriveDropRule::TryGetReceivingDrive (deskDrive->model, GetHardDiskBlocks (deskDrive->slot, deskDrive->drive), outDrive);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::IsDropAccepted
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::IsDropAccepted (int tag, const ImageClass & image)
{
    ReceivingDrive  drive;
    DriveAddress    address;
    bool            hasDrive = TryGetDropDrive (tag, drive, address);



    return hasDrive && DriveImageRule::Accepts (drive, image);
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

    if (!connected)
    {
        m_drivePickers->Close (DriveAddress { 0, 0, true });
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
    ChangeHardDiskImage (drive.slot, drive.drive, ShellDeskScene::GetDriveTitle (drive), path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ChangeHardDiskImage
//
//  As above, for a drive given by its address and title, as a drive's picker
//  has it.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ChangeHardDiskImage (int slot, int drive, const std::wstring & driveTitle, const std::wstring & path)
{
    int  choice = 0;



    m_chrome->HideTooltips();

    choice = ShowModalDialog (MakeHardDiskChangeDialog (driveTitle, fs::path (path).filename().wstring()));

    if (choice == kHardDiskChangeRestart)
    {
        PostCommand (IDM_HARDDISK_ATTACH, std::format ("{} {} {}", slot, drive, TextEncoding::WideToUtf8 (path)));
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
            action    = std::format (L"Swapping {} for {}", GetPortDriveText (device), GetPortDriveText (DiskPortChainRule::GetSwapDevice (device)));
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
                state.AddDiskPortDrive (DiskPortChainRule::GetSwapDevice (device));
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

        if (!DiskPortChainRule::GetSwapDevice (device).empty())
        {
            auto  swap = std::make_shared<DxuiCommand>();



            swap->label     = (device == kpszApple35DriveDevice) ? L"S&wap for a 5.25-inch drive" : L"S&wap for a 3.5-inch drive";
            swap->dispatch  = [this, device, nth] () { ChangePortDrive (PortDriveChange::Swap, device, (size_t) nth); };
            swap->isVisible = [this, device] () { return DiskPortChainRule::CanSwap (m_machine.GetConfig(), device); };

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
//  The drive's picker, whose pick is confirmed and attached as a drop is.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::BrowseHardDisk (const DeskDrive & drive)
{
    m_drivePickers->OpenForDeskDrive (drive);
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
    HRESULT  hr = EditSavedMachine ([&edit] (SettingsPanelState & state)
    {
        edit (state);
        return S_OK;
    });



    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::EditSavedMachine
//
//  Reads the running machine's saved settings, applies `edit`, and writes
//  them back, through the same document Settings writes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::EditSavedMachine (const std::function<HRESULT (SettingsPanelState & state)> & edit)
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
    JsonParseError      parseErr;
    std::ifstream       configFile;
    std::stringstream   ss;



    BAIL_OUT_IF (m_settings->GetConfigStore() == nullptr || configPath.empty(), HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    configFile.open (configPath);
    BAIL_OUT_IF (!configFile.good(), HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    ss << configFile.rdbuf();

    hr = JsonParser::Parse (ss.str(), defaultJson, parseErr);
    CHRA (hr);

    hr = SettingsPanelState::EditSaved (*m_settings->GetConfigStore(), m_settings->GetFileSystem(), machineNameNarrow, defaultJson, edit);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ShowStorageContextMenu
//
//  Shows a device's own menu on a right-click: the switch that connects it,
//  then the Storage menu's commands for that device. Drive 1 has no such
//  switch; the machine's first drive is always present.
//
//  The rows are the Storage menu's commands, so their enabled state and
//  check marks match the menu's. The disk's rows are worded for a menu the
//  drive's name heads, as a 3.5-inch drive's is.
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

    // A //c Plus's drive 2 commands act on its first 5.25-inch drive, and its
    // second has only insert and eject.
    if (device != kStorageMenuRecorder && DiskMenuRule::HasBuiltIn35Drive (m_machine.GetConfig()))
    {
        if (device != 0)
        {
            ShowDriveContextMenu (DeskDrive { DeskDriveModel::Disk2c, BootDriveRule::kDiskIiSlot, device, false, true }, x, y);
            return;
        }

        ids = { IDM_DISK_INSERT2, IDM_DISK_EJECT2, IDM_DISK_WP2, IDM_DISK_SALVAGE2, IDM_DISK_INSPECT2, IDM_DISK_THUMBNAIL2 };
    }
    else if (device == kStorageMenuRecorder)
    {
        ids = { IDM_STORAGE_RECORDER, 0, IDM_TAPE_INSERT, IDM_TAPE_PLAY, IDM_TAPE_STOP, IDM_TAPE_REWIND,
                IDM_TAPE_FASTFORWARD, IDM_TAPE_EJECT };
    }
    else
    {
        // Headed and worded as a 3.5-inch drive's menu is: the drive's name,
        // then the disk's commands.
        bool  isFirst = (device == 0);

        items.push_back (DxuiPopupMenuItem::ForHeader (ShellDeskScene::GetDriveTitle (m_scene->GetDiskIiDrive (device))));

        if (!isFirst)
        {
            items.push_back (DxuiPopupMenuItem::ForCommand (commands.Find (IDM_STORAGE_DRIVE2)));
            items.push_back (DxuiPopupMenuItem::ForSeparator());
        }

        items.push_back (DxuiPopupMenuItem::ForCommand (MakeRelabeled (commands.Find (isFirst ? IDM_DISK_INSERT1    : IDM_DISK_INSERT2),    L"&Insert disk...")));
        items.push_back (DxuiPopupMenuItem::ForCommand (MakeRelabeled (commands.Find (isFirst ? IDM_DISK_EJECT1     : IDM_DISK_EJECT2),     L"&Eject disk")));
        items.push_back (DxuiPopupMenuItem::ForCommand (commands.Find (isFirst ? IDM_DISK_WP1      : IDM_DISK_WP2)));
        items.push_back (DxuiPopupMenuItem::ForCommand (commands.Find (isFirst ? IDM_DISK_SALVAGE1 : IDM_DISK_SALVAGE2)));
        items.push_back (DxuiPopupMenuItem::ForCommand (MakeRelabeled (commands.Find (isFirst ? IDM_DISK_INSPECT1   : IDM_DISK_INSPECT2),   L"Inspect &disk...")));
        items.push_back (DxuiPopupMenuItem::ForCommand (MakeRelabeled (commands.Find (isFirst ? IDM_DISK_THUMBNAIL1 : IDM_DISK_THUMBNAIL2), L"Save screenshot as the disk's thu&mbnail")));

        // Drive 1 is always there, so its menu is where a detached drive 2 or
        // recorder is attached again: neither is on screen to be clicked.
        if (isFirst && m_disks->IsSecondDriveOffered() && !m_disks->ShouldShowExternalDrive())
        {
            ids = { 0, IDM_STORAGE_DRIVE2 };
        }

        if (isFirst && m_tapeDeck->MachineHasCassettePort() && !m_tapeDeck->IsTapeRecorderShown())
        {
            if (ids.empty())
            {
                ids.push_back (0);
            }

            ids.push_back (IDM_STORAGE_RECORDER);
        }
    }

    for (int id : ids)
    {
        items.push_back (id == 0 ? DxuiPopupMenuItem::ForSeparator() : DxuiPopupMenuItem::ForCommand (commands.Find (id)));
    }

    ids.clear();

    // On the //c Plus the 5.25-inch drives are on the disk port's chain. Only
    // the last one is unplugged, since a disk is saved by its drive's number.
    if (DiskPortChainRule::IsOffered (m_machine.GetConfig()) && (device == 0 || device == 1))
    {
        int  floppies = DiskPortChainRule::CountDrives (m_machine.GetConfig(), kpszDiskIicDevice);



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
//  A floppy drive's or hard disk's menu: the drive's name, then for a
//  floppy drive its insert and eject, and for a hard disk its image
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

    if (drive.model == DeskDriveModel::Disk2c)
    {
        auto  insert = std::make_shared<DxuiCommand>();
        auto  eject  = std::make_shared<DxuiCommand>();



        insert->label     = L"&Insert disk...";
        insert->dispatch  = [this, drive] () { m_disks->BrowseForDisk (drive.drive); };
        eject->label      = L"&Eject disk";
        eject->dispatch   = [this, drive] () { m_disks->Eject (drive.slot, drive.drive); };
        eject->isEnabled  = [this, drive] () { return !m_disks->GetMountedImagePath (drive.drive).empty(); };

        items.push_back (DxuiPopupMenuItem::ForCommand (insert));
        items.push_back (DxuiPopupMenuItem::ForCommand (eject));
    }
    else if (ShellDeskScene::IsFloppy35 (drive))
    {
        auto  insert = std::make_shared<DxuiCommand>();
        auto  eject  = std::make_shared<DxuiCommand>();



        insert->label     = L"&Insert disk...";
        insert->dispatch  = [this, drive] () { m_scene->BrowseDeskDrive (drive); };
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
    if (drive.slot == 0 && DiskPortChainRule::IsOffered (m_machine.GetConfig()))
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
